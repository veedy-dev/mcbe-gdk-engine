# MCBE GDK engine v0.2.0-ex

This is an **experimental, byte-for-byte mirror** of LukasPAH's tested
`GDK-Proton-10-32-Custom-4` release. It is published under this repository so
the MCBE GDK installer can expose one exact, reviewed engine target without
following a moving external release.

## Exact upstream identity

- Repository: `LukasPAH/GDK-Proton-Custom`
- Tag: `release-10-32-4`
- Asset: `GDK-Proton10-32-Custom-4.tar.gz`
- SHA-256: `4d19774c64451d4f1395dc4c5f4b6e8b5fdbc1ce6c05e29a855f5e0678b8800c`
- Declared WineGDK source line: `LukasPAH/WineGDK`, branch `minimal-xbl`

The release workflow verifies the SHA-256 and archive structure before
publishing. The engine archive is not rebuilt or modified, so it remains the
same binary that was tested upstream. A machine-readable provenance JSON is
attached to the release.

## Sign-in behavior

This engine intentionally uses **in-game Microsoft sign-in** rather than the
installer's launcher/CLI authentication flow:

1. Launch Minecraft and select **Sign In** inside the game.
2. The WineGDK remote-connect callback writes the verification URL and user code
   to `login.json`.
3. A compatible MCBE GDK installer supervises that request, opens or displays
   the Microsoft device-code prompt, and waits while the game completes the
   credential refresh.
4. Account switching and sign-out are performed from Minecraft's Profile UI.

The matching installer integration must also apply the engine-specific
`MicrosoftGame.Config` identity and remote-login handling. Installing this
archive by itself is not sufficient.

## Known limitations

The upstream release currently reports that **Parties and Realms do not work**
because required GDK components are still incomplete. Treat this release as an
opt-in performance and sign-in experiment, preserve the previous stable engine,
and report results separately for launch time, server join time, frame pacing,
and server tick time.

This prerelease does not replace the source provenance of the stable
`mcbe-gdk-engine` builds. A future stable engine should source-port the useful
remote-connect implementation into the maintained tree and pass the normal
build, security, feature, and reproducibility checks.
