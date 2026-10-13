# Stabilising for releases

A release cycle ends with a stable release which also starts a new cycle and
lifts any code freezes. Gradual code freezing towards a stable release starts
with an alpha release. The release stages of a cycle are:

- **Alpha release**:
    Signified by version number #.#.91.
    Major features must have landed before this. Major features include
    invasive code motion and refactoring, high risk changes, and new stable
    library ABI.

- **Beta release**:
    Signified by version number #.#.92.
    Minor features must have landed before this. Minor features include all
    new features that are not major, low risk changes, clean-ups, and
    documentation. Stable ABI that was new in the alpha release can be removed
    before a beta release if necessary.

- **Release candidates (RC)**:
    Signified by version number #.#.93 and up to #.#.99.
    Bug fixes that are not release critical must have landed before this.
    Release critical bug fixes can still be landed after this, but they may
    call for another RC.

- **Stable release**:
    Signified by version number #.#.0.
    Ideally no changes since the last RC.

Mind that version #.#.90 is never released. It is used during development when
no code freeze is in effect. Stable branches and point releases are not covered
by the above.

# Releasing

To make a release of Weston, follow these steps.

0. Verify the test suites and codebase checks pass.  All of the tests should
   either pass or skip.

       ninja -C build/ test

1. Verify that the wayland and wayland-protocols version dependencies are
   correct, and that wayland-protocols has had a release with any needed
   protocol updates.

2. Update the first stanza of `meson.build` to the intended version.

   If the ABI has been broken, make sure `libweston_major` has been bumped since
   the last release.

   Then commit your changes:

       RELEASE_NUMBER="x.y.z"
       RELEASE_NAME="[alpha|beta|RC1|RC2|official|point]"
       git status
       git commit meson.build -m "build: bump to version $RELEASE_NUMBER for the $RELEASE_NAME release"
       git push

3. Ensure that you have glab client installed locally and are authenticated to
   freedesktop.org Gitlab instance. You can do that using the following:

       glab auth login

   Use gitlab web interface to generate a token either on Weston project or
   on your profile (preferably) and use the token to authenticate. gitlab cli
   client can be downloaded directly from Gitlab.com or it might be provided
   directly by the distribution you're currently using to do the release.

4. Run the `release.sh` script to generate the tarballs, sign and upload them,
   and generate a release announcement template. This script can be obtained
   from the Wayland repository:

   https://gitlab.freedesktop.org/wayland/wayland/-/blob/main/release.sh

5. Compose the release announcements.  The script will generate a
   weston-x.y.z.announce file with a list of changes and tags.  Prepend these
   with a human-readable listing of the most notable changes.  For x.y.0
   releases, indicate the schedule for the x.y+1.0 release.

6. PGP sign the release announcement and send it to
   <wayland-devel@lists.freedesktop.org>.

7. Update `releases.html` in wayland.freedesktop.org with links to tarballs and
   the release email URL.

   Once satisfied:

       git add releases.html releases/weston-${RELEASE_NUMBER}.tar.xz*
       git commit -m "releases: add weston ${RELEASE_NUMBER} release"
       git push

8.  For x.y.0 releases, also create the release series x.y branch.

The x.y branch is for bug fixes and conservative changes to the x.y.0 release,
and is where we create x.y.z releases from.  Creating the x.y branch opens
up main for new development and lets new development move on.  We've done
this both after the x.y.0 release (to focus development on bug fixing for
the x.y.1 release for a little longer) or before the x.y.0 release
(like we did with the 1.5.0 release, to unblock main development early).

    git branch x.y [sha]
    git push origin x.y

The main branch's `meson.build` version should always be (at least) x.y.90,
with x.y being the most recent stable branch.  The stable branch's `meson.build`
version is just whatever was most recently released from that branch.

For stable branches, we commit fixes to main first, then `git cherry-pick -x`
them back to the stable branch.
