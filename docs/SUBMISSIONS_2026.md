# FreeEQ8 submission tracker — 2026 index campaign

_Last cross-checked: 2026-07-31_

This file tracks public submission routes for FreeEQ8 so PRs, directory listings, editorial pitches, product launches, forum posts, and SEO backlinks do not duplicate work.

**This file is the source of truth as of 2026-07-31.** The earlier cross-check at [`docs/outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md`](outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md) is superseded and retained for history only — several of its conclusions have since been contradicted by direct API checks.

## Audit-backed positioning snapshot

FreeEQ8 currently presents a public JUCE/C++ audio plugin package with:

- VST3 / AU / Standalone targets for FreeEQ8, plus ProEQ8 commercial target scaffolding.
- 8-band parametric EQ positioning for FreeEQ8.
- Linear phase engine, dynamic EQ controls, match EQ, mid/side processing, oversampling, per-band drive/saturation, spectrum FIFO/analyzer, presets, level meter, update checker, and release workflow files.
- macOS / Linux / Windows build packaging scripts or workflows.
- Public release docs, milestone report, tester call, test matrix, outreach templates, screenshots, Ableton screenshots, and featured/submission trackers.
- Standalone regression evidence: `Tests/AuditRegressionTest.cpp` and `Tests/BiquadTest.cpp` compile and pass outside the JUCE build when tested locally from this package.

## Current indexable SEO phrase set

Use these phrases across titles, tags, release copy, directory descriptions, and article headings:

- FreeEQ8
- free EQ plugin
- free parametric EQ plugin
- free VST3 EQ
- free AU EQ plugin
- open-source EQ plugin
- JUCE EQ plugin
- C++ audio plugin
- free FabFilter Pro-Q alternative
- free TDR Nova alternative
- dynamic EQ plugin
- linear phase EQ plugin
- match EQ plugin
- mid/side EQ plugin
- spectrum analyzer EQ
- free mixing plugin
- free mastering EQ
- audio DSP open source
- TizWildin Plugin Ecosystem
- Gary Doman FreeEQ8
- GareBear99 FreeEQ8

## Current status snapshot

| Status | Count | Meaning |
|---|---:|---|
| Verified third-party public wins | 9 | Rekkerd, AudioApp.cn, Midifan.com, AudiArtist.com, OpenAudio, awesome-music-production, Open Audio Stack/StudioRack registry, Audinux Fedora packaging, geekaifree/BookShelf. |
| Self-maintained listing | 1 | Useful SEO, not third-party validation. |
| Declined or dead | 3 | awesome-audio-engineering (declined with reasons), awesome-webaudio (closed), awesome-audio-dsp (claim does not resolve). |
| Active/open PRs | 5 | 3 corrected or answered on 2026-07-31; 2 sitting in dormant repos. |
| Withdrawn 2026-07-31 | 3 | awesome-musicdsp, awesome-linuxaudio, awesome-open-synth — structurally unwinnable. |
| Direct email submissions sent | 8+ | Outreach sent; no confirmed coverage reply found except Rekkerd and KVR routing. Follow-up windows lapsed in late May. |
| Manual/account targets | 6+ | Highest-value places needing direct account/form action. |
| Off-target technical outreach | 2 | iPlug2 and DISTRHO/DPF are not current fits for a JUCE plugin. |

**Campaign result 2026-04-23 → 2026-07-31: zero merges from submissions.** Every new win in this period came from third parties adopting FreeEQ8 independently (Audinux packaging, Open Audio Stack registry, AudiArtist review). No follow-up action was taken on any submission between 2026-05-30 and 2026-07-31.

## Verified public wins — safe to cite

| Target | Status | Evidence |
|---|---:|---|
| Rekkerd.org | ✅ Featured | Live article: `FREE: FreeEQ8 parametric EQ effect plugin by Gary Doman`, published 2026-05-19. |
| AudioApp.cn | ✅ Screenshot-verified coverage | Chinese/English audio-community article/thread: `Bonus: Gary Doman launches FreeEQ8 free and open source parametric equalizer effects plugin` / `福利：Gary Doman 推出 FreeEQ8 免费开源的参量均衡器效果插件`; links to official GitHub repo. |
| Midifan.com | ✅ Verified coverage | Major Chinese audio technology outlet; evidence in `docs/press/MIDIFAN_COVERAGE.md`. |
| AudiArtist.com | ✅ Long-form review | `FreeEQ8: Free Dynamic EQ VST3 for Mixing` — most detailed third-party write-up to date; discusses linear-phase limitations and Match EQ caveats accurately. |
| webprofusion / OpenAudio | ✅ Listed | Current public OpenAudio page shows `FreeEQ8` in Audio Plugins; Issue #207 is closed. |
| ad-si / awesome-music-production | ✅ Listed | PR #197 merged 2026-03-16; current public README shows `FreeEQ8` under Apps. |
| Open Audio Stack / StudioRack | ✅ Live plugin page | https://open-audio-stack.github.io/open-audio-stack-registry/plugins/garebear99/freeeq8/ — confirmed live, shows v2.3.0 / Neo-VECTR / GPL-3.0 / Mac .dmg. Backed by `src/plugins/garebear99/freeeq8/2.3.0/index.yaml`; also in `studiorack/studiorack-cli` test snapshots. Install: `studiorack plugin install garebear99/freeeq8`. |
| Audinux (Fedora COPR) | ✅ Third-party packaging | Packaged as `freeeq8` — versions 2.1.0-1, 2.2.0-1, 2.3.0-1 recorded in `pages/news.md`. Independent packaging; makes the Linux story materially more honest than "build from source". |
| geekaifree / BookShelf | ✅ Listed (Chinese) | `music-production.md`: `免费开源 8 段参量均衡插件 (VST3/AU)`. |
| GareBear99 / awesome-audio-plugins-dev | ✅ Self-maintained listing | Useful index route; not third-party validation. |

## Completed/closed evidence — do not overclaim as live listing yet

| Target | Conservative status | Evidence / reason |
|---|---:|---|
| BillyDM / awesome-audio-dsp | ❌ Claim does not resolve | Issue #14 returns `Could not resolve to an Issue with the number of 14` from the GitHub API. Remove this from any credibility copy. |
| sudara / awesome-juce | 🟡 Conditional yes on record | PR #61 closed 2026-05-12 with an explicit offer to add it if resubmitted short and per instructions. Resubmission is #64 — see active table. |
| notthetup / awesome-webaudio | ❌ Dead | Issue #83 CLOSED. Maintainer invited a PR on 2026-03-27; none was ever raised. Do not revive — wrong technology domain. |
| brandonhimpfen / awesome-audio-engineering | ❌ Declined, with reasons | PR #5 closed 2026-05-14 citing *"heavily self-promotional in presentation... cross-promotional 'awesome list' networks"*. Same maintainer's `resourcerank` tool ships FreeEQ8 as its worked example at 61/100, *"highly promotional"*. |

## Active PRs / issues

| Target | Status | Link | Follow-up rule |
|---|---:|---|---|
| sudara / awesome-juce #64 | ⏳ Open — corrected 2026-07-31 | https://github.com/sudara/awesome-juce/pull/64 | Highest-probability item. Was editing the nightly-generated `README.md`; now a single `sites.txt` line under `## Effects`. Maintainer already agreed in principle. Wait 3–4 weeks, then one polite nudge maximum. |
| landscape82 / awesome-sound-design-resources #3 | ⏳ Open — corrected 2026-07-31 | https://github.com/landscape82/awesome-sound-design-resources/pull/3 | Repo actively maintained (pushed 2026-07-30). Description trimmed from ~175 to ~110 chars to match neighbours. Wait. |
| noteflakes / awesome-music #109 | ⏳ Open — ball in maintainer's court | https://github.com/noteflakes/awesome-music/pull/109 | Scope question answered 2026-07-31 with an offer to withdraw. Do not push further; accept whichever way they call it. |
| dreikanter / awesome-vst #18 | 💤 Dormant repo | https://github.com/dreikanter/awesome-vst/pull/18 | Upstream last pushed 2024-09-15. Leave open, invest nothing. |
| twinysam / FreeAudioPluginList #11 | 💤 Dormant repo | https://github.com/twinysam/FreeAudioPluginList/pull/11 | Upstream last pushed 2024-11-28. Leave open, invest nothing. |

## Withdrawn 2026-07-31

| Target | Reason | Resubmit? |
|---|---|---|
| olilarkin / awesome-musicdsp #11 | `CONTRIBUTING.md`: *"a personal curated awesome list... I am not looking for collaborators."* | Never. |
| nodiscc / awesome-linuxaudio #71 | GitHub repo is a `[mirror]`; contributions require a **GitLab merge request** at `gitlab.com/nodiscc/awesome-linuxaudio`. | Yes, via GitLab, using their documented entry syntax. Now genuinely justified since Audinux packages it for Fedora. |
| detroitsynth / awesome-open-synth #3 | Weak fit (EQ, not synth); repo dormant since 2023-06-09. | No. |

## Direct email submissions sent

These were sent as direct outreach. Do not resend unless there is a real update, release build, packaged installer, demo video, benchmark/test report, or 10–14 day follow-up window has passed.

| Target | Status | Recipient / route | Sent date | Follow-up rule |
|---|---:|---|---|---|
| Rekkerd | ✅ Covered | `ronnie@rekkerd.org` | 2026-05-16 | Follow up only for major version, ProEQ8, or new demo video. |
| KVR Audio | 🟡 Routing reply | `contactus@kvraudio.com` | 2026-03-25 and 2026-05-16 | Create developer account and product listing; email alone is not enough. |
| Bedroom Producers Blog | ⏳ Sent | `tomislav@bedroomproducersblog.com` | 2026-03-26, 2026-05-05, 2026-05-16 | Send one refined follow-up with Rekkerd/OpenAudio/awesome-music-production proof + demo assets. |
| ProducersBuzz | ⏳ Sent | `submit@producersbuzz.com` | 2026-05-12 | Wait / follow once after release polish. |
| ProducerSpot | ⏳ Sent | `info@producerspot.com` | 2026-05-16 | Wait. |
| Home Music Maker | ⏳ Sent | `info@homemusicmaker.com` | 2026-05-16 | Wait. |
| Gearnews | ⏳ Sent | `news@gearnews.com` | 2026-05-16 | Wait. |
| MusicTech / NME / Guitar.com | ⏳ Sent | editorial addresses | 2026-05-05 | Low priority unless story becomes broader. |
| Sound On Sound | ⏳ Sent | SOS editorial contacts | 2026-05-05 | Follow only with stable installer/demo. |
| Tutorials Dojo | ⏳ Sent | `support@tutorialsdojo.com` | 2026-05-16 | Better for edge/local audio AI article angle. |

## Immediate manual/account targets — do next

| Target | Priority | Action | Recommended indexing angle | Required assets before submitting |
|---|---:|---|---|---|
| KVR Audio Product Database | A+ | Create Developer Account; add FreeEQ8 product; submit news item | `FreeEQ8 — free open-source JUCE/C++ parametric EQ plugin, VST3/AU/Standalone` | Logo/screenshot, release URL, platform list, version, short/long descriptions. |
| Audio Plugins for Free | A | Upload / submit plugin | `Free VST/AU EQ plugin for mixing and mastering` | Direct download/release link, screenshot, OS/format list. |
| Plugins4Free | A | Submit/list plugin if route is active | `Free EQ effect plugin for Cubase, FL Studio, REAPER, Ableton and VST/AU hosts` | Stable download link and install notes. |
| AlternativeTo | A | Suggest new application | `Free and open-source alternative to FabFilter Pro-Q, TDR Nova, ZL Equalizer` | Platforms, GPL-3.0 license, tags, description, screenshot. |
| Product Hunt | A- | Schedule launch | `Free open-source EQ plugin for producers and DSP developers` | Hero image, screenshots, launch comment, maker profile, demo GIF/video. |
| Hacker News / Show HN | A- | Submit only when a major release page/demo is clean | `Show HN: FreeEQ8 – a free open-source JUCE/C++ EQ plugin` | GitHub repo, concise technical comment, no vote requests. |
| REAPER Forum / r/Reaper | A | Forum post | `Free open-source EQ plugin tested in REAPER; looking for host feedback` | REAPER screenshot, VST3 install notes. |
| KVR Forum thread | A | Developer/product thread after product listing | `FreeEQ8 — open-source EQ with linear phase, dynamic EQ, match EQ, M/S, analyzer` | KVR product page first. |
| LinuxMusicians | B+ | Post after Linux package/build path is clean | `Open-source JUCE EQ with Linux VST3 build path` | Honest Linux status and build instructions. |

## Technical outreach status

| Target | Status | Correct interpretation |
|---|---:|---|
| JUCE Forum/support | 🟡 Relevant but narrow asks only | Forum is relevant, but future asks should be specific and not broad “audit my repo” requests. |
| Tracktion/pluginval | ⏳ Relevant validation route | Keep focused on plugin validation and host compatibility. |
| Chowdhury-DSP/BYOD | 🟡 Paid expert review offered | Jatin offered review at $85/hour; useful optional expert feedback, not listing/coverage. |
| DISTRHO/DPF | ❌ Off-target | FreeEQ8 is JUCE, not DPF; avoid unless there is a DPF/LV2 port. |
| iPlug2 | ❌ Off-target | Maintainer said it does not seem iPlug2-relevant; avoid unless there is an iPlug2 port/comparison. |

## Best current truth statement for future submissions

> FreeEQ8 is a free, open-source JUCE/C++ 8-band parametric EQ plugin with VST3/AU/Standalone packaging and public source code. Third-party coverage: Rekkerd.org, AudiArtist.com, AudioApp.cn and Midifan.com. Listed on OpenAudio and awesome-music-production, registered in the Open Audio Stack / StudioRack plugin registry, and independently packaged for Fedora by Audinux.

## Next actions

Ordered by expected return, highest first.

1. **Reduce promotional surface in the README.** This is the single recurring objection from every maintainer who gave a reason, and it gates everything below. Move the ecosystem banner, social links, giveaway counter and ProEQ8 purchase badges below the technical content.
2. **Cite Audinux packaging.** Independent Fedora packaging is stronger third-party evidence than any list entry, and it replaces the weaker "Linux: build from source" positioning.
3. Wait on `awesome-juce#64` and `awesome-sound-design-resources#3` — both corrected 2026-07-31. One nudge each after 3–4 weeks, then stop.
4. Resubmit awesome-linuxaudio via **GitLab merge request** (not GitHub), now that Fedora packaging exists to support the entry.
5. Create / complete KVR Developer Account product listing.
6. Follow up with Bedroom Producers Blog — the 10–14 day window lapsed in late May, so this is now a fresh pitch, not a follow-up. Lead with Rekkerd + AudiArtist + Audinux.
7. Submit to Audio Plugins for Free, Plugins4Free, AlternativeTo, and Product Hunt only after release/demo assets are clean.
8. **Before any future submission, read the target's CONTRIBUTING file first.** Three of the eight 2026 submissions failed on mechanics rather than merit: one edited an auto-generated file, one went to a read-only mirror, one went to a list that states it accepts no contributions.
