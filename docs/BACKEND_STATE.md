# Backend, auth and checkout state

**Verified 2026-07-31.** Every claim below was checked against the live service or
the current `main` of the relevant repo on that date. Where something was *not*
found, that is stated as an absence rather than left implied.

This document exists so the next release cycle does not have to re-derive any of
it. It covers what blocks ProEQ8 from being purchasable and what blocks a DAW
from checking a real account.

---

## 1. Summary

A **purchasable ProEQ8** is close. It needs credentials and a small amount of
wiring; nothing has to be invented.

A **unified Google account shared between the website and the DAW** is not close.
It does not exist in any form yet. The commonly held assumption that hub Google
auth already exists and only needs syncing is incorrect — see §4.

These are independent. Checkout does not depend on Google auth, and should not be
sequenced behind it.

---

## 2. What is live and working

The licensing Worker is deployed and responding:

- Host: `https://tizwildin-hub.admension.workers.dev`
- Referenced from the plugin at `Source/Config.h:26` (`kActivationServerURL`)
- `POST /activate`, `/verify`, `/deactivate` all respond, returning structured
  validation errors for malformed keys
- `GET /hub/account?email=<addr>` responds `HTTP 200` with JSON shaped
  `{email, hubAccount, licenses, masterKey}`

So activation and verification infrastructure genuinely exists. The plugin-side
counterpart in `Source/LicenseValidator.h` is complete: offline HMAC pre-check,
server verification, 30-day offline grace, machine binding.

---

## 3. Live security issue: unauthenticated account read

`GET /hub/account?email=<anything>` returns `HTTP 200` with **no credential of
any kind**. Identity is a client-supplied query parameter.

Confirmed 2026-07-31 with a deliberately non-existent address, which returned:

```json
{"email":"...@example.com","hubAccount":null,"licenses":[],"masterKey":null}
```

The fields are null only because that account does not exist. The response shape
shows the endpoint is designed to return `licenses` and `masterKey` for an
address that does exist. Anyone who can guess or harvest a customer email can
therefore request that customer's licence data.

This was **not** probed with a real address, because doing so would mean reading
someone else's account data. The shape of the response is sufficient evidence;
exploiting it is not necessary and was not done.

Severity is raised by two things: the `masterKey` field name implies a
credential rather than a status flag, and email addresses are not secret.

**This should be closed before any marketing drives traffic to the hub**, and it
is independent of every other item here. The minimal fix is to stop accepting
email as identity and require a bearer token on that route.

---

## 4. Correction: there is no Google auth in the hub repo

`GareBear99/TizWildinEntertainmentHUB` is public and contains 740 tracked paths.
Searching the full tree for `auth`, `oauth`, `google`, `openid`, `token` and
`session` returns **no Google or OAuth implementation**.

The only auth implementation is `arc_service/app/services/auth_service.py`, which
is **72 lines** and exposes:

- `mock_login(account_id, machine_id)`
- `local_register(email, password, machine_id, display_name)`
- `local_login(email, password, machine_id)`
- `local_refresh(refresh_token, machine_id)`
- `resolve_token(token)`
- `validate_token(token)`
- `revoke_token(token)`

There is no Google client ID, no ID-token verification, no JWT handling, and no
OpenID discovery anywhere in the file or the repo.

The token *lifecycle* is well shaped — `resolve`/`validate`/`revoke` plus
`machine_id` binding is the right model, and it is a better design than what is
actually deployed. But it is a local-password prototype, and the presence of
`mock_login` alongside it indicates it was built for development.

Every data file backing it is a mock:

```
arc_service/app/data/auth_tokens.mock.json
arc_service/app/data/entitlements.mock.json
arc_service/app/data/seats.mock.json
...
```

There is no database, and no deployment of `arc_service` was found.

**Consequence:** "sync the DAW with the hub's Google auth" is not a wiring task.
Google auth has to be built first, on both sides. Treat it as new work.

---

## 5. Stripe products do not exist

The repo contains only intent, not configuration:

- `stripe/PRODUCTS_TO_CREATE.md`
- `stripe/stripe.env.example`
- `manifests/stripe.mapping.json`

`manifests/stripe.mapping.json` carries real pricing intent (seat subscription at
$3/month, max quantity 9, recurring quantity billing) but its own notes say:

> "Fill real Stripe product and price IDs after dashboard setup."

No live product or price IDs are present anywhere.

---

## 6. Checkout host does not resolve

`server/pages/proeq8/checkout.js:19` points at
`https://proeq8-checkout.tizwildin.workers.dev`.

```
Host proeq8-checkout.tizwildin.workers.dev not found: 3(NXDOMAIN)
```

Still NXDOMAIN as of 2026-07-31. This is consistent with §5: the checkout worker
was never deployed because the products it would reference were never created.

---

## 7. Credentials are not available to tooling on this machine

Checked 2026-07-31 on the development machine:

- `wrangler` — **not installed**
- `stripe` CLI — **not installed**
- `gh` — installed and authenticated
- Environment variables matching `cloudflare`, `stripe`, `wrangler`, `CF_*`,
  `GOOGLE_CLIENT*`, `OAUTH` — **zero matches**
- No `~/.config/.wrangler` and no `~/.config/stripe`

Account-level access may well exist in a browser session, but no credential is
reachable by command-line tooling, so no deploy, secret rotation, or product
creation can be performed from here. Both CLIs would need installing and
authenticating first.

---

## 8. The published signing secret

`Source/LicenseValidator.h (145-164)` embeds the HMAC-SHA256 signing secret,
XOR-0x5A obfuscated, in this public GPL-3.0 repository. XOR against a constant is
encoding, not encryption.

The secret must be treated as **published**, and every key ever issued under it as
**forgeable**. Rotation is necessary regardless of any other decision here, and
it requires the Cloudflare access described in §7.

The durable fix is asymmetric signing: the server signs entitlements with a
private key and the plugin verifies with a public key, which is safe to ship in
plaintext in GPL source. `juce_cryptography` is already linked by both targets and
provides `RSAKey`. Note this does not stop someone forking the source and deleting
the check — that is not solvable and should be accepted rather than engineered
against.

---

## 9. Distance to each outcome

Ordered by dependency, not by size. Effort assumes credentials are in hand.

### Outcome A — ProEQ8 is purchasable
1. Create Stripe products and prices, fill real IDs into
   `manifests/stripe.mapping.json` — needs Stripe access
2. Deploy a checkout endpoint, or replace the flow with a Stripe Payment Link and
   drop the dead host from `checkout.js` — needs Cloudflare access, or nothing at
   all if a Payment Link is acceptable
3. Handle `checkout.session.completed` and issue a licence key — needs both

This is the shortest path to revenue and needs **no Google auth**. A Stripe
Payment Link plus manual key issuance removes step 2 entirely and is the fastest
working configuration.

### Outcome B — the hub stops leaking account data
4. Require a bearer token on `/hub/account` and stop treating email as identity
   (§3) — needs Cloudflare access

Independent of A. Should not wait for it.

### Outcome C — one account works on the website and in the DAW
5. Build Google OAuth server-side, keyed on the immutable `sub` claim rather than
   email — new work, does not exist (§4)
6. Add Google sign-in to the hub front end — new work
7. Decide the authoritative backend: the deployed Worker or `arc_service`. They
   disagree, and the stronger design is the one that is not deployed. Building
   further before deciding means building twice
8. Give `arc_service` a real datastore, replacing the `.mock.json` files, and
   deploy it — or port its token model into the Worker
9. Get a token into the plugin. Preferred is the OAuth 2.0 device authorization
   grant (RFC 8628), which Google supports natively. Worth shipping first is a
   link code pasted into the existing licence-key field, which is a drop-in
   replacement with no new user-facing friction and can be upgraded later without
   changing the entitlement model. Avoid a localhost loopback redirect — plugin
   hosts sandbox aggressively and it triggers firewall prompts
10. Swap HMAC for asymmetric signing and rotate the compromised secret (§8)

This is the large one. Step 7 gates 8, 9 and 10, and it is a decision rather than
an implementation.

### Outcome D — sponsor entitlements
11. Link `github_id` to the account, query `sponsorshipsAsMaintainer`, grant Pro
    above a threshold. Requires the `read:user` scope
    (`gh auth refresh -s read:user`). No sponsor logic exists anywhere today.

Depends on Outcome C. Policy still undecided: whether a one-time sponsorship
grants permanent access, whether cancellation revokes it, and how private
sponsors are treated.

---

## 10. Recommended order

1. **§3, the account-data leak.** Live, unauthenticated, and cheap to fix.
2. **Outcome A via a Stripe Payment Link.** Makes ProEQ8 purchasable with the
   least infrastructure, and needs no Cloudflare deploy.
3. **§8, rotate the signing secret.** Every key issued so far is forgeable.
4. **Step 7, decide Worker versus `arc_service`.** A decision, not a build, and
   everything in Outcome C depends on it.
5. **Outcome C.** Only after 4, and expect it to be the largest item on the
   roadmap.

Nothing in this list requires Google auth to exist before ProEQ8 can be sold.

---

## 11. Related unresolved item

`github-recovery-codes.txt` was observed in plaintext under
`Desktop/Projects/Main projects/`. GitHub account recovery codes in a plaintext
file on a development machine are worth moving into a password manager. Not
related to the backend work, but noted here because it surfaced while mapping
credential availability and is a larger exposure than anything else in this
document.
