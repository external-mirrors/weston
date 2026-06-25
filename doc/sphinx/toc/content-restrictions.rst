Weston Content Restrictions
===========================

Weston has several facilities for consuming and presenting protected content,
such as rights managed video.

Protocols
---------

weston-restricted-buffer - A weston protocol for flagging buffer contents as
``restricted``. ``restricted`` is a warning to the compositor that the buffer
contents are allocated in protected memory and can't be safely read by the
renderer without a protected context.

weston-content-protection - A weston protocol for indicating that a surface
requires a protected connection between the video source and sink. The
requested level of protection may be strictly ``enforced`` (in this case
the compositor needs to censor content if the protection it supports does
not meet the client criteria), or the current level of protection may be
relayed to the client in a ``relaxed`` mode where the client may use it
perform its own content censoring, such as renderingat a reduced resolution.


Configuration
-------------

For weston's renderer to handle restricted buffers, the renderer must be
capable of using a restricted context, and the renderer must be configured
to use a restricted context, which can be done with:

.. code-block:: ini

    [core]
    renderer-restricted-context=true

Further, per-output policy must be enabled such as only allowing restricted
buffers to be rendered when an output has HDCP2 content protection, with:

.. code-block:: ini

    [output]
    name=HDMI-A-1
    restriction-policy=hdcp2

To allow the compositor to use HDCP, the output must be configured
appropriately:

.. code-block:: ini

    [output]
    name=HDMI-A-1
    allow_hdcp=true

Consult the weston.ini man page for more detail.

Test clients
------------

Weston provides some simple test clients for the protection and restriction
protocols.

``weston-content_protection`` is a trivial shm-buffer using application that
demonstrates the weston-content-protection protocol, in both the relaxed and
enforced modes.

``weston-simple-dmabuf-egl`` is a general dmabuf test client that has two
command line options for testing buffer restrictions:

.. code-block:: console

  -p   allocates buffers in protected memory
  -r   uses weston-restricted-buffer protocol

They're separated so intentionally harmful states can be exercised. For
example, a buffer can be allocated in protected memory with no
indication to the compositor, which will lead to an attempt to render the
protected buffer. Similarly, an unprotected buffer can be flagged as
restricted, in order to test policy configuration on platforms that lack a
protected memory facility.
