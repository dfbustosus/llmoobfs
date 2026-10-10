# ADR-0004 — Open-source foundation and documentation

Date: 2026-10-10. Status: adopted.

The user requested an open-source GitHub repository, README, Pages, contribution
policies and durable project memory. They explicitly selected **GPL-2.0-only for
the entire project** and **claudio@studiomoob.com** for security/conduct reports.

Use an English entry README and Portuguese companion, a maintainer-led governance
model, contribution/security/conduct policies, issue and PR templates, and minimal
GitHub Actions permissions. Pin external actions by commit. Test native code in
QEMU. Keep product claims tied to executed evidence and link the future proposal
separately. Maintain a static, dependency-free GitHub Pages site in `site/`, combining
a visual explanation with direct access to technical documentation, as requested.

No CLA or DCO process is introduced at this stage. A simple original conduct policy
avoids implying an independent moderation team that the project does not have.
Git and remote copies preserve exact history; journals retain rationale summaries.
CI artifacts expire and are not the sole historical record.

Research informed the scope rather than imposing every optional repository feature:

- [Open Source Guides: starting a project](https://opensource.guide/starting-a-project/)
- [GitHub community health files](https://docs.github.com/en/communities/setting-up-your-project-for-healthy-contributions/creating-a-default-community-health-file)
- [GitHub Pages custom workflows](https://docs.github.com/en/pages/getting-started-with-github-pages/using-custom-workflows-with-github-pages)
- [Linux kernel license rules](https://docs.kernel.org/process/license-rules.html)
- [Claude project memory](https://code.claude.com/docs/en/memory)
- [Cursor rules](https://cursor.com/docs/rules)

Publication and repository settings are tracked separately in the journal according
to the actual remote state; preparing a workflow does not mean deployment succeeded.
