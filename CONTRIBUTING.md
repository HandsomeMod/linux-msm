# Contributing to msm8916-mainline/linux
Thanks for your interest in contributing to msm8916-mainline/linux! :tada:

## About
[msm8916-mainline/linux] is a fork of the [mainline Linux kernel](https://kernel.org)
(released by Linus Torvalds). Goal of the repository is to **temporarily** host
**work-in-progress** changes for various devices based on the
Qualcomm Snapdragon 410/412 (MSM8916) SoC.

Once ready, changes should be [submitted upstream](https://www.kernel.org/doc/html/latest/process/submitting-patches.html)
through the appropriate mailing lists.  
**Changes submitted in this repository will not automatically land upstream!**

**Note:** This fork is **unofficial** and not (directly) affiliated to the mainline
Linux project. As such, it is entirely **optional** to submit your changes as
pull request to this repository. You can also immediately send your changes
upstream to the appropriate mailing lists. Submitting your changes here (before
sending them upstream) has the following advantages:

  - **Preliminary review:** You get early feedback on your patches which may
    speed up submission of your patch upstream later.
    **Note:** Acceptance of your patch in this repository does not mean that
    the upstream maintainers will accept it!

  - **Kept up-to-date:** All patches in this repository will be rebased to newer
    upstream Linux releases, so your device will be always up-to-date even with
    some work-in-progress changes.

  - **Packaged in [postmarketOS]:** This fork is the source of the
    `linux-postmarketos-qcom-msm8916` package in postmarketOS, which is used by
    all MSM8916 devices based on the mainline kernel. Your changes will be
    included in the next release and therefore show up in postmarketOS.

## Patch Requirements
Goal for all patches in this repository is to upstream them eventually. Therefore,
the formal requirements are similar to upstream, e.g.:

  - No compile warnings/errors
  - Clean code style
  - Changes separated into clean commits (one per logical change/subsystem)
  - Appropriate commit message that explains the motivation for the change

If you want to contribute to this repository, you should be willing to bring your
patches into suitable shape for upstreaming. Most of this is documented upstream
in [Submitting patches]. Don't worry if you don't get it exactly right the first time. :)

### Sign off your patches - the Developer's Certificate of Origin
Upstream contributions to the Linux kernel are required to have the following
tag in the commit message:

```
Signed-off-by: FirstName LastName <your-email@example.com>
```

These tags are also required when contributing to this repository. Reason for
this is that you might become busy at some point, or your interests change. Then
it is important that someone else can pick up your work and finish it up.

Please read [Sign your work - the Developer’s Certificate of Origin](https://www.kernel.org/doc/html/latest/process/submitting-patches.html#sign-your-work-the-developer-s-certificate-of-origin)
**carefully** to understand the meaning of the `Signed-off-by:`.
It also explains how to add it easily when creating new commits.

## Upstreaming
Upstreaming changes takes time. And sometimes, changes cannot be submitted
upstream yet because they are work-in-progress or because there are fundamental
open problems that cannot be solved immediately, e.g.:

  - Weird issues where the actual cause cannot be determined at the moment ("hacks").
  - New drivers with problems in some edge cases.
  - Panel drivers that are mostly auto-generated and that are hard to document
    properly because of lack of documentation.
  - Battery/charging drivers that are hard to validate without expert knowledge.

If you have something (mostly) working and the [formal patch requirements](#patch-requirements)
are met, then it's good to share it with others in this repository.

However, keep in mind that maintaining a large amount of patches in this
repository consumes a significant amount of time, especially when breaking
changes are made upstream. This means less time to review new patches
or to work on improvements everyone can benefit from.

**Please help to keep maintenance time at an acceptable level by submitting your
patches upstream when they are ready.** If you are not sure if your patches are
ready, just ask and we can discuss it.

### Patches that touch upstream files
Patches that touch upstream files which are frequently updated
(e.g. existing, shared drivers instead of files specific to your device)
should be submitted upstream before they are merged into this repository.
This is because those tend to cause conflicts much more frequently when updating
to newer kernel versions.

This is just a guideline to reduce maintenance, exceptions can be made if necessary.

## Questions
If you have any questions, feel free to ask in the [postmarketOS mainline channel
on Matrix or IRC](https://wiki.postmarketos.org/wiki/Matrix_and_IRC).

[msm8916-mainline/linux]: https://github.com/msm8916-mainline/linux
[postmarketOS]: https://postmarketos.org
[Submitting patches]: https://www.kernel.org/doc/html/latest/process/submitting-patches.html
