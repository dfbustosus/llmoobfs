#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-2.0-only
"""Check static HTML links and fragments without network dependencies."""
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1] / 'site'


class Page(HTMLParser):
    def __init__(self, path):
        super().__init__()
        self.ids = set()
        self.links = []
        self.feed(path.read_text())

    def handle_starttag(self, tag, attrs):
        data = dict(attrs)
        if 'id' in data:
            assert data['id'] not in self.ids, f"Duplicate ID: {data['id']}"
            self.ids.add(data['id'])
        for attribute in ('href', 'src'):
            if attribute in data:
                self.links.append(data[attribute])


pages = {path: Page(path) for path in ROOT.rglob('*.html')}
assert pages, 'No site pages found'
for source, page in pages.items():
    for link in page.links:
        parsed = urlsplit(link)
        if parsed.scheme or parsed.netloc:
            continue
        assert not parsed.path.startswith('/'), f'Root-relative link breaks project Pages: {link}'
        target = (source.parent / unquote(parsed.path)).resolve() if parsed.path else source
        assert target.is_relative_to(ROOT), f'Link outside site: {link}'
        assert target.exists(), f'{source.name}: missing {link}'
        if parsed.fragment:
            assert target in pages and unquote(parsed.fragment) in pages[target].ids, f'{source.name}: missing anchor {link}'
print(f'Site links valid: {len(pages)} HTML pages')
