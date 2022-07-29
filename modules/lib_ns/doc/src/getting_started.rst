Getting Started
===============

Overview
--------

``lib_ns`` is a library which performs Noise Suppression (NS), by estimating the noise and 
subtracting it from frame. ``lib_ns`` library functions make use of functionality 
provided in ``lib_xs3_math`` to perform DSP operations. For more details, refer to :ref:`ns_overview`.


Repository Structure
--------------------

* ``modules/lib_ns`` - The actual ``lib_ns`` library directory within ``https://github.com/xmos/fwk_voice/``.
  Within ``lib_ns``

  * ``api/`` - Headers containing the public API for ``lib_ns``.
  * ``doc/`` - Library documentation source (for non-embedded documentation) and build directory.
  * ``src/`` - Library source code.


Requirements
------------

``lib_ns`` is included as part of the ``fwk_voice`` github repository and all requirements for cloning
and building ``fwk_voice`` apply. ``lib_ns`` is compiled as a static library as part of the overall
``fwk_voice`` build. It depends on `lib_xs3_math <https://github.com/xmos/lib_xs3_math/>`_.


Getting and Building
--------------------

This module is part of the parent ``fwk_voice`` repo clone. It is compiled as a static library as part of
``fwk_voice`` compilation process.

To include ``lib_ns`` in an application as a static library, the generated ``libfwk_voice_module_lib_ns.a`` can then be linked
into the application. Add ``lib_ns/api`` to the include directories when building the application.
