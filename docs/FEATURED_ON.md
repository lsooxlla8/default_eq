# FreeEQ8 — Featured, listed, and submitted

_Last cross-checked: 2026-07-31_

This file separates **verified public wins** from **pending submissions** and **closed/completed evidence that still needs a public-page recheck**. Do not claim a community/listing as live unless it appears in the verified section below.

## Verified public wins — safe to cite

| Source | Status | Notes |
|---|---:|---|
| [Rekkerd.org](https://rekkerd.org/free-freeeq8-parametric-eq-effect-plugin-by-gary-doman/) | ✅ Featured | Article: `FREE: FreeEQ8 parametric EQ effect plugin by Gary Doman`, published 2026-05-19. Gmail confirms Ronnie accepted the submission and acknowledged the version typo correction. |
| [AudioApp.cn](https://www.audioapp.cn/thread-237167-1-1.html) | ✅ Screenshot-verified coverage | Chinese/English audio-community pickup titled `Bonus: Gary Doman launches FreeEQ8 free and open source parametric equalizer effects plugin` / `福利：Gary Doman 推出 FreeEQ8 免费开源的参量均衡器效果插件`. Screenshot evidence stored in `docs/press/`; page names Gary Doman, FreeEQ8, feature set, ProEQ8, and the official GitHub URL. |
| [Midifan.com](https://www.midifan.com/modulenews-detailview-59241.htm) | ✅ Verified coverage | Major Chinese audio technology media outlet. Names Gary Doman, FreeEQ8, full feature set, ProEQ8, and links to official GitHub. Evidence in `docs/press/MIDIFAN_COVERAGE.md`. |
| [webprofusion / OpenAudio](https://github.com/webprofusion/OpenAudio) | ✅ Listed | Current OpenAudio public page shows `FreeEQ8` in Audio Plugins. Issue [#207](https://github.com/webprofusion/OpenAudio/issues/207) is closed. |
| [ad-si / awesome-music-production](https://github.com/ad-si/awesome-music-production#apps) | ✅ Listed | [PR #197](https://github.com/ad-si/awesome-music-production/pull/197) merged on 2026-03-16; current public README shows `FreeEQ8` under Apps. |
| [StudioRack / Open Audio Stack](https://open-audio-stack.github.io/open-audio-stack-registry/plugins/garebear99/freeeq8/) | ✅ Live plugin page | Public page confirmed live (HTTP 200), showing `FreeEQ8 v2.3.0`, author `Neo-VECTR`, GPL-3.0, Equalizer tag, and a Mac `.dmg` download. Backed by `src/plugins/garebear99/freeeq8/2.3.0/index.yaml` in `open-audio-stack/open-audio-stack-registry`, with demo audio and image assets. Install: `studiorack plugin install garebear99/freeeq8`. Also present in `studiorack/studiorack-cli` committed test snapshots. |
| [Audinux (Fedora COPR)](https://audinux.github.io/) | ✅ Third-party packaging | Packaged independently as `freeeq8`; `pages/news.md` records 2.1.0-1, 2.2.0-1 and 2.3.0-1. Packaged by the Audinux maintainers, not submitted by us — the strongest independent-adoption signal to date. |
| [AudiArtist.com](https://www.audiartist.com/freeeq8-free-dynamic-eq-vst3-plugin/) | ✅ Long-form review | Full review: `FreeEQ8: Free Dynamic EQ VST3 for Mixing`. Covers dynamic EQ, linear-phase limitations, Match EQ caveats, M/S and oversampling. Most detailed third-party write-up so far. |
| [geekaifree / BookShelf](https://github.com/geekaifree/BookShelf) | ✅ Listed (Chinese) | `music-production.md` lists FreeEQ8 as `免费开源 8 段参量均衡插件 (VST3/AU)`. Third Chinese-language pickup after AudioApp.cn and Midifan.com. |
| [孤狼音频 / guaud.com](https://guaud.com/119.html) | ✅ Listed (Chinese) | Curated list of free legitimate 64-bit VST3 effects for streaming and mixing. Entry reads `FreeEQ8 (8 段参量动态均衡器)`. Notable for the company it keeps — ZL Equalizer 2, Pultec EQP-1A / API 550A / LA-2A / Fairchild 660 emulations — indicating an editorially curated list rather than an SEO scrape. Fourth Chinese-language pickup. |
| [GareBear99 / awesome-audio-plugins-dev](https://github.com/GareBear99/awesome-audio-plugins-dev#equalizers) | ✅ Self-maintained listing | Useful SEO/index route, but mark as self-maintained rather than third-party validation. |

## Closed/completed evidence — not safe to market as live-listed until rechecked

| Source | Conservative status | Notes |
|---|---:|---|
| [BillyDM / awesome-audio-dsp](https://github.com/BillyDM/awesome-audio-dsp) | ❌ Claim does not hold up | Issue #14 does not resolve via the GitHub API (`Could not resolve to an Issue with the number of 14`) — it either never existed or is not an issue. Public README does not show `FreeEQ8`. **Do not cite this anywhere.** |
| [sudara / awesome-juce](https://github.com/sudara/awesome-juce) | 🟡 Conditional yes on record | PR #61 closed 2026-05-12 with `Happy to add this project but please open a new PR that follows the instructions and shorten the description`. That is an explicit conditional acceptance. See the active table for the corrected resubmission. |
| [notthetup / awesome-webaudio](https://github.com/notthetup/awesome-webaudio/issues/83) | ❌ Dead | Issue #83 confirmed CLOSED as of 2026-07-31. Maintainer asked for a PR on 2026-03-27; no PR was ever raised and the invitation lapsed. Weak fit regardless — FreeEQ8 is JUCE/VST3/AU, not Web Audio. |
| [brandonhimpfen / awesome-audio-engineering](https://github.com/brandonhimpfen/awesome-audio-engineering) | ❌ Declined, with reasons | PR #5 closed 2026-05-14. Maintainer's stated reason, verbatim: the project appears *"relatively early-stage, heavily self-promotional in presentation, and closely tied to a broader ecosystem of related repositories, monetization links, and cross-promotional 'awesome list' networks"*. He added the assessment may change *"as the project matures and establishes a stronger independent reputation"*. See `resourcerank` note below. |

## Active review / pending submissions

| Source | Status | Notes |
|---|---:|---|
| [sudara / awesome-juce #64](https://github.com/sudara/awesome-juce/pull/64) | ⏳ Open — corrected 2026-07-31 | **Best odds of the remaining set.** Originally edited `README.md`, which the repo regenerates nightly from `sites.txt` via `update_readme.rb` — so the change would have been wiped even if merged. It also landed in the Hosts & DAWs table rather than Effects, left the star/freshness columns blank and did not bump the entry count. Now a single line in `sites.txt` under `## Effects`, satisfying all five checklist items. |
| [landscape82 / awesome-sound-design-resources #3](https://github.com/landscape82/awesome-sound-design-resources/pull/3) | ⏳ Open — corrected 2026-07-31 | Repo is actively maintained (pushed 2026-07-30). Format was correct but the description ran ~175 characters against a neighbour average of ~60; rebased on current main and trimmed. |
| [noteflakes / awesome-music #109](https://github.com/noteflakes/awesome-music/pull/109) | ⏳ Open — awaiting maintainer scope call | `@levinericzimmermann` asked on 2026-05-27 whether the list should open an `Audio Plugins` category and tagged `@noteflakes`; that question went unanswered for nine weeks. Answered directly on 2026-07-31, including an explicit offer to withdraw if plugins are out of scope. |
| [dreikanter / awesome-vst #18](https://github.com/dreikanter/awesome-vst/pull/18) | 💤 Open but repo dormant | Open since 2026-04-23 with no maintainer response. Upstream repo last pushed **2024-09-15**. Left open; do not invest further effort. |
| [twinysam / FreeAudioPluginList #11](https://github.com/twinysam/FreeAudioPluginList/pull/11) | 💤 Open but repo dormant | No comments from anyone. Upstream repo last pushed **2024-11-28**. Left open; do not invest further effort. |

## Withdrawn 2026-07-31 — structurally unwinnable

Closed voluntarily rather than left rotting in maintainer queues.

| Source | Reason for withdrawal |
|---|---|
| [olilarkin / awesome-musicdsp #11](https://github.com/olilarkin/awesome-musicdsp/pull/11) | `CONTRIBUTING.md` states plainly: *"This is a personal curated awesome list... I am not looking for collaborators."* The list does not accept submissions at all. Never resubmit. |
| [nodiscc / awesome-linuxaudio #71](https://github.com/nodiscc/awesome-linuxaudio/pull/71) | The GitHub repo is explicitly marked `[mirror]` and its homepage points to GitLab. Contributions must go through a **GitLab merge request** at `gitlab.com/nodiscc/awesome-linuxaudio`. The PR was filed somewhere that cannot merge it. Entry format is documented in their CONTRIBUTING if resubmitting. |
| [detroitsynth / awesome-open-synth #3](https://github.com/detroitsynth/awesome-open-synth/pull/3) | Weak fit (EQ effect, not a synth) and the repo has been dormant since **2023-06-09**. |

## Email outreach / editorial submissions

| Target | Status | Notes |
|---|---:|---|
| Bedroom Producers Blog | ⏳ Email sent | Sent multiple times; no confirmed reply found. Best follow-up target after adding Rekkerd link + demo assets. |
| ProducersBuzz | ⏳ Email sent | Submitted to `submit@producersbuzz.com` on 2026-05-12. |
| ProducerSpot | ⏳ Email sent | Submitted to `info@producerspot.com` on 2026-05-16. |
| Home Music Maker | ⏳ Email sent | Submitted to `info@homemusicmaker.com` on 2026-05-16. |
| Gearnews | ⏳ Email sent | Submitted to `news@gearnews.com` on 2026-05-16. |
| MusicTech / NME / Guitar.com | ⏳ Email sent | Sent 2026-05-05; lower priority unless the story expands beyond a plugin listing. |
| Sound On Sound | ⏳ Email sent | Review request sent 2026-05-05; needs polished release/demo follow-up only. |
| KVR Audio email | 🟡 Routing reply | KVR replied with developer-account route; product listing itself is still not complete. |

## Manual high-value gaps

| Target | Priority | Next action |
|---|---:|---|
| KVR Audio Product Database | Highest | Create Developer Account, add FreeEQ8 product listing, submit news item. |
| Audio Plugins for Free | High | Submit once stable direct download/release page and screenshots are ready. |
| Plugins4Free | High | Submit if route is active; use VST3/AU/free EQ wording. |
| AlternativeTo | High | Create/suggest FreeEQ8 profile as free/open-source EQ alternative. |
| Product Hunt | Medium-high | Launch after hero image/demo/release assets are ready. |
| Hacker News / Show HN | Medium-high | Post as technical JUCE/C++/DSP build story, not a promo blast. |
| REAPER Forum / r/Reaper | High | Post with REAPER-specific testing notes and screenshots. |

## Public technical testing / DSP review outreach

| Target | Status | Notes |
|---|---:|---|
| JUCE Forum / support | 🟡 Relevant but narrow asks only | JUCE restored account and said forum is the right route, but warned broad review requests ask a lot of people’s time. |
| Tracktion/pluginval | ⏳ Relevant validation route | Keep as plugin validation / host-test focused. |
| Chowdhury-DSP/BYOD | 🟡 Paid expert feedback offered | Jatin offered technical review at $85/hour; this is not free validation/listing. |
| DISTRHO/DPF | ❌ Off-target | FreeEQ8 is JUCE, not DPF. Do not repeat unless a DPF/LV2 port exists. |
| iPlug2 community | ❌ Off-target | Maintainer said it does not seem iPlug2 relevant. Do not repeat unless an iPlug2 port/comparison exists. |

## Evaluated and deliberately excluded

Found during the 2026-07-31 sweep and judged **not** to be listings. Recorded so they are not re-investigated or mistakenly cited as wins.

| Mention | Why excluded |
|---|---|
| `EdgeAgent/EDGE-AGENCY-AI-Governance` | AI-governance pipeline using FreeEQ8 as its **first live test case** (`docs/OPERATOR_EVIDENCE.md`, "Portfolio issue #1"). Evidence of being evaluated, not endorsed. |
| `brandonhimpfen/resourcerank` | Submission-scoring tool shipping FreeEQ8 as its worked example at 61/100. Feedback, not a listing — see section below. |
| `AriESQ/stars` | Personal starred-repo archive containing a copy of OpenAudio's README. Mirror of an existing listing, not a new one. |
| `relatedrepos.com/gh/GareBear99/FreeEQ8` | Auto-generated SEO aggregator page scraped from GitHub metadata. No editorial curation. |
| `dev.to/tizwildin` ×2 | Self-published articles. Legitimate SEO surface but self-authored — never cite as third-party validation. |
| Own repos (ARC-Core, PAP-Forge-Audio, awesome-music-platforms, TizWildin-Obsidian, WURP, Free-Violin-Synth-Sample-Kit) | Self-referential cross-links within the TizWildin ecosystem. Part of the "cross-promotional network" maintainers flagged. |

## Verification coverage and limits

What the 2026-07-31 sweep actually covered, so future readers know what is and is not proven:

- **Covered:** exhaustive GitHub code search for `FreeEQ8` (all 108 matches across both result pages), direct GitHub API state checks on every tracked PR and issue, and English + Chinese + Japanese + Korean web searches.
- **Confirmed absent:** no listing on KVR Audio, AlternativeTo, VST4Free, Plugins4Free or Audio Plugins For Free. Nobody has added FreeEQ8 to these unsolicited — they remain genuine opportunities, not existing wins.
- **Asian coverage is exactly four, all Chinese:** Midifan.com, AudioApp.cn, 孤狼音频/guaud.com, geekaifree/BookShelf. No Japanese or Korean coverage found. (Corrected — this line previously read "exactly three" and omitted guaud.com, contradicting the verified-wins table above it.)
- **All ten verified-win URLs re-checked 2026-07-31 and returning HTTP 200.** Note that `rekkerd.org` returns **403 to a plain-UA request** and only serves the article with full browser headers; it is live (173 KB, article body intact), so do not mistake a 403 from a scripted check for a dead link.
- **Not covered — treat as unknown:** non-GitHub forges (GitLab, Codeberg), forums (KVR, REAPER, LinuxMusicians, Gearspace), Reddit, YouTube, Discord, and any Chinese forum content behind a login.
- **Not verifiable without inbox access:** every claim in this file sourced from Gmail — notably the Rekkerd acceptance note and the KVR routing reply, plus all ten email pitches in the outreach table. These rest on the original author's reading of their inbox and were **not** re-confirmed in this sweep.

## Recurring maintainer feedback — presentation, not DSP

Three independent signals converge on the same point. None of them criticise the engineering.

- **brandonhimpfen** (awesome-audio-engineering #5, 2026-05-14) declined it as *"heavily self-promotional in presentation, and closely tied to a broader ecosystem of related repositories, monetization links, and cross-promotional 'awesome list' networks"*.
- **sudara** (awesome-juce #61, 2026-05-12) asked specifically to *"shorten the description"*.
- **`brandonhimpfen/resourcerank`** — the same maintainer subsequently built a submission-scoring tool and ships **FreeEQ8 as its worked example** (`examples/freeeq8-review-input.json`). Committed output scores it **61/100**, summary: *"Main issue: Resource presentation appears highly promotional."* Suggested action: *"Request clarification, stronger documentation, licensing details, or additional adoption evidence before accepting."*

The README currently leads with the ecosystem banner, SoundCloud/YouTube/Facebook links, a user-count giveaway tracker and Buy-ProEQ8 badges before reaching technical content. Awesome-list maintainers weight the opposite way. Reducing promotional surface above the fold is likely to unblock more than any additional submission volume.

Supporting evidence: the two genuine wins this cycle — Audinux packaging and the Open Audio Stack registry entry — both came from third parties adopting the plugin independently, not from the submission campaign, which produced zero merges between 2026-04-23 and 2026-07-31.

## Reusable verified credibility copy

### Short

> FreeEQ8 is a free, open-source JUCE/C++ 8-band parametric EQ plugin featured by Rekkerd.org, AudioApp.cn, Midifan.com and AudiArtist.com, listed on OpenAudio and awesome-music-production, registered in the Open Audio Stack / StudioRack plugin registry, and independently packaged for Fedora by Audinux.

### Directory / listing

> FreeEQ8 is a free, open-source JUCE/C++ parametric EQ plugin for VST3/AU/Standalone workflows, with 8-band EQ, linear phase, dynamic EQ direction, match EQ direction, M/S processing, per-band drive/saturation direction, oversampling, and real-time spectrum analysis.

Superseded history: [docs/outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md](outreach/CROSSCHECKED_SUBMISSION_STATUS_2026-05-21.md) — retained for reference, but several of its conclusions were contradicted by the 2026-07-31 API re-check. This file supersedes it.
