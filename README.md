<!--
  - SPDX-FileCopyrightText: 2017 Nextcloud GmbH and Nextcloud contributors
  - SPDX-FileCopyrightText: 2011 Nextcloud GmbH and Nextcloud contributors
  - SPDX-License-Identifier: GPL-2.0-or-later
-->
# Unofficial fork: custom request headers

This fork lets the Nextcloud desktop client send custom HTTP headers with every request to your Nextcloud server, for example the access token of an authenticating reverse proxy. Headers can be added or changed for an existing account without logging in again. See [nextcloud/desktop#8819](https://github.com/nextcloud/desktop/issues/8819) and, for the Android app, [nextcloud/android#5821](https://github.com/nextcloud/android/issues/5821).

## Using it

1. Download a build:
   1. Open the [successful builds](https://github.com/wayneyaoo/desktop/actions/workflows/custom-build-packages.yml?query=is%3Asuccess+branch%3Abuild%2Fcustom-request-headers) and click the top one, which is the latest.
   2. At the bottom of the page under *Artifacts*, download `nextcloud-windows-msi` for Windows or `nextcloud-appimage` for Linux. Each is a zip with the installer or AppImage inside.
   3. Downloading needs a GitHub login, and builds expire after 90 days.
2. Install it. On Windows, the MSI replaces an installed official client and keeps your settings. It is unsigned, so choose *More info* → *Run anyway* when SmartScreen warns.
3. Enter the headers, one `Name: value` per line:
   - New account: *Custom headers* in the setup wizard, before logging in.
   - Existing account: *Settings* → your account → *Connection settings* → *Custom Headers* → *Apply*.

The source is on the [`feat/custom-request-headers`](https://github.com/wayneyaoo/desktop/tree/feat/custom-request-headers) branch.

## Disclaimer

- **Not official.** This is not a Nextcloud release and is not supported by Nextcloud.
- **Where to report.** Report problems with this feature in [this fork's issues](https://github.com/wayneyaoo/desktop/issues), not to Nextcloud, as their client doesn't contain this code.
- **Security.** Don't trust my builds. This client holds your credentials and files, so review [the changes against upstream](https://github.com/nextcloud/desktop/compare/master...wayneyaoo:desktop:build/custom-request-headers), including the build workflow, for example with an AI assistant. Then build it yourself: fork this repository with all branches (untick *Copy the master branch only*), enable Actions, and push a commit to `build/custom-request-headers`.
- **Tested platforms.** It is tested only as an AppImage on Debian 13 with KDE, and on Windows 11, where it works. Other systems should work but are untested.
- **Keychain.** Header values are stored in the system keychain, like the proxy password: Credential Manager on Windows, Keychain on macOS, KWallet on KDE or GNOME Keyring on GNOME. Without a keychain the client is designed to keep working, but this is barely tested and may behave oddly.
- **AI-written.** The code was written with AI. I'm an engineer and followed best practices with care, but I don't know C++. Use at your own risk.
- **Upstream.** I intend to keep this up to date with upstream, and I'm happy to work with the Nextcloud team if they want the patch. I won't push for it, as they said in the issue that it's not planned.
- **Scope.** Requests unrelated to custom headers won't be handled. Related but non-essential changes will likely be declined too, since every extra change makes keeping up with upstream harder.
- **Platform issues.** I'll do my best to help with platform-specific problems with this feature, but I'd encourage you to build your own fork, which AI makes cheap today. Adding more here would make it harder to keep up with upstream.

---

# Nextcloud Desktop Client

[![REUSE status](https://api.reuse.software/badge/github.com/nextcloud/desktop)](https://api.reuse.software/info/github.com/nextcloud/desktop)

The Nextcloud Desktop Client is an app to synchronize files from Nextcloud Server with your computer available for Windows, macOS and Linux.

<p align="center">
    <img src="doc/images/main_dialog_christine.png" alt="Desktop Client on Windows" width="450">
</p>

## Downloads 🚀
For the latest stable and recommended version, please refer to [the official download page](https://nextcloud.com/install/#install-clients).

## Help 🛟
You can find [the user, administration and developer manuals for the desktop client](https://docs.nextcloud.com/#desktop) on our central documentation site.

## Contributing 🫴
- Make sure to follow our [guidelines for contributing](https://github.com/nextcloud/desktop/blob/98690b1e9141f2c602c9b4583c1f9ed16b95a309/CONTRIBUTING.md) to this repository.
- Don't forget to read our [Code of Conduct](https://nextcloud.com/community/code-of-conduct/). This document offers some guidance to ensure Nextcloud participants can cooperate effectively in a positive and inspiring atmosphere and to explain how together we can strengthen and support each other.

## Join the team 👪
There are many ways to contribute, of which development is only one! Find out [how to get involved](https://nextcloud.com/contribute/), including as a translator, designer, tester, helping others, and much more! 😍

## Help testing 🔬
Download and install the client:

- [All releases](https://github.com/nextcloud-releases/desktop/releases)<br>
- [Daily builds](https://download.nextcloud.com/desktop/daily)

## Reporting issues 🐛
If you find any bugs or have any suggestion for improvement, please
[open an issue in this repository](https://github.com/nextcloud/desktop/issues).

## Bug fixing and development 🛠️

> [!TIP]
> For contributors on macOS, see the [macOS development guide](./doc/macOS-development.md).

> [!NOTE]  
> Find the system requirements and instructions on [how to work with KDE Craft in our desktop client blueprints repository](https://github.com/nextcloud/craft-blueprints-nextcloud/).

### System requirements
- Windows 10, Windows 11, macOS 13 Ventura (or newer) or Linux
- [🔽 Inkscape (to generate icons)](https://inkscape.org/release/)
- Developer tools: cmake, clang/gcc/g++:
- Qt6 since 3.14, Qt5 for earlier versions
- OpenSSL
- [🔽 QtKeychain](https://github.com/frankosterfeld/qtkeychain)
- SQLite
- [Xcode](https://developer.apple.com/xcode/) (only on macOS)

Optional recommendations:

- [Qt Creator IDE](https://www.qt.io/product/development-tools)
- [delta: A viewer for git and diff output](https://github.com/dandavison/delta)

### Build

Step by step instructions on how to build the client to contribute.

1. Clone the Github repository: `git clone https://github.com/nextcloud/desktop.git`
2. Create build directory: `mkdir <build directory>`
3. Navigate into build directory: `cd <build directory>`
4. Compile: `cmake -S <cloned desktop repo> -B build -DCMAKE_PREFIX_PATH=<dependencies> -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX=. -DNEXTCLOUD_DEV=ON`

> [!TIP]
> The cmake variable NEXTCLOUD_DEV allows you to run your own build of the client while developing in parallel with an installed version of the client.

Then you might continue with these steps:
	
1. 🐛 [Pick a good first issue](https://github.com/nextcloud/desktop/labels/good%20first%20issue)
2. 👩‍🔧 Create a branch and make your changes. Remember to sign off your commits using `git commit -sm "Your commit message"`
3. ⬆ Create a [pull request](https://opensource.guide/how-to-contribute/#opening-a-pull-request) and `@mention` the people from the issue to eview
4. 👍 Fix things that come up during a review
5. 🎉 Wait for it to get merged!

### Test servers

The easiest way to have a local Nextcloud server to develop, debug and test the client against is [the Nextcloud Docker image](https://github.com/nextcloud/docker).
The following example shows how to deploy a Nextcloud Docker container on the local host which will be removed again as soon as the command is interrupted.
Note that this requires Docker to be installed in your developer environment.

```bash
docker run \
    --rm \
    --publish 8080:80 \
    --env SQLITE_DATABASE=nextcloud.sqlite \
    --env NEXTCLOUD_ADMIN_USER=admin \
    --env NEXTCLOUD_ADMIN_PASSWORD=admin \
    nextcloud
```

This simple test server already suffices in the most cases. For more advanced server test deployments we also recommend [Nextcloud development environment on Docker Compose](https://juliusknorr.github.io/nextcloud-docker-dev/).

## Get in touch 💬
* [📋 Forum](https://help.nextcloud.com)
* [🐘 Mastodon](https://mastodon.xyz/@nextcloud)
* [🔗 LinkedIn](https://www.linkedin.com/company/nextcloud-gmbh/)
* [🦋 Bluesky](https://bsky.app/profile/nextcloud.bsky.social)
* [👥 Facebook](https://www.facebook.com/nextclouders)

You can also [get professional support for Nextcloud and the desktop client](https://nextcloud.com/support)!

## License 📜

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful, but
    WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
    or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
    for more details.
