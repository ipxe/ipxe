Contributing to iPXE
====================

Thank you for wanting to help to improve iPXE!  Your contributions can
help to make the software better for everyone.

iPXE is [licensed][licence] under the GNU GPL and is 100% open source
software.  There are no "premium edition" versions, no in-app
advertisements, no hidden costs.  The version that you get to download
and use forever for free is the definitive and only version of the
software.

There are several ways in which you can contribute:

Bug reports
-----------

Good quality [bug reports][issues] are always extremely welcome.
Please describe exactly what you are trying to achieve, and the exact
problem that you are encountering.  Include a screenshot or a
transcription of the output from iPXE (including the exact text of any
error messages).

If you are not sure whether or not your issue is due to a bug in iPXE,
please consider creating a [discussion][discussions] instead of an
issue.

Bug fix pull requests
---------------------

Short and simple [pull requests][pulls] that fix a known [bug][issues]
are always welcome.  Please ensure that your pull request allows
maintainer edits, since there is a 99% chance that it will need to be
rebased, reworded, or otherwise modified before being merged.

Minor feature pull requests
---------------------------

A small [pull request][pulls] that adds a very minor feature has some
chance of being merged.

As a bootloader, iPXE is an unusual programming environment: code size
is extremely small (measured in bytes or kilobytes), you absolutely
cannot use any external libraries, heap size is very limited and you
should expect memory allocations to fail, various operations may not
be used in some contexts (e.g. on network fast paths), and cooperative
multitasking is implemented using event-driven non-blocking object
interfaces.  You will need to be aware of external interoperability
requirements and firmware quirks dating back to the 1990s.

In most cases, an [issue report][issues] describing the desired
feature is likely to get a better result than an unsolicited pull
request.

Major feature pull requests
---------------------------

A large [pull request][pulls] that adds a substantial new feature
(e.g. a new device driver, a new cryptographic algorithm, a new script
command, etc) has very little chance of being merged unless an
appropriate level of [funding][funding] is provided to cover the time
required for several rounds of code review.

Please be aware that thorough code review is generally **more work**
than code creation.  The cost to review a major feature will therefore
generally be more expensive than the cost of simply [funding][funding]
the development of the feature instead.

Any externally developed features are **not eligible** to be signed
for UEFI Secure Boot, and will be automatically excluded from all
Secure Boot builds of iPXE.

Funding
-------

Small donations from individual users via [GitHub Sponsors][sponsors]
are always welcome, and help to purchase the coffee that forms the raw
material from which iPXE is made.

Around half of the development work on iPXE is commercially funded.
Any code that is commercially funded will be pushed to the public iPXE
repository and made immediately available to everyone under the terms
of the GNU GPL.  Funded development has included many widely used
features:

  * Support for 64-bit CPUs
  * Support for UEFI firmware
  * Support for USB NICs
  * Support for the Intel 10GbE, 40GbE, and 100GbE NICs
  * Support for [iSCSI and other SAN booting methods][sanboot]
  * Support for [TLS and HTTPS][crypto]
  * Support for [AWS EC2][ec2] and [Google Cloud][gce] public clouds

As noted above, funding the development of a major feature is
generally cheaper than developing it yourself and then funding the
code review to get it fixed up and merged.

Any funded development work is automatically eligible to be signed for
UEFI Secure Boot.

To discuss funding an iPXE development, please send an email to
<vendor-support@ipxe.org>, or create an [issue][issues] and mention
that you would be interested in providing funding.


[crypto]: https://ipxe.org/crypto
[discussions]: https://github.com/ipxe/ipxe/discussions
[ec2]: https://ipxe.org/howto/ec2
[funding]: #funding
[gce]: https://ipxe.org/howto/gce
[issues]: https://github.com/ipxe/ipxe/issues
[licence]: COPYING
[pulls]: https://github.com/ipxe/ipxe/pulls
[sanboot]: https://ipxe.org/cmd/sanboot
[sponsors]: https://github.com/sponsors/ipxe
