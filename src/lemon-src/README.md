The source files in this directory are Lemon by D. Richard Hipp who
disclaims copyright per comments in the source code.

--

The Makefile here is not normally invoked during the pmtr build.
This is only manually invoked if cfg.y (the grammar) is changed. In
that situation the author should run make in this directory to build
the cfg.c and cfg.h parser sources. Those sources, once generated,
are checked into the repository; they are NOT regenerated during a
typical build.

While its possible to include the generation of cfg.c and cfg.h from
cfg.y as part of the standard build, doing so causes some hassle with
cross-compiled environments, where the lemon executable would be cross
compiled for a target architecture, but then needs to be executed during
the build to generate cfg.c and cfg.h. (But the host architecture cannot
execute it, because it cross compiled lemon for another architecture).

It is much simpler to pretend that cfg.c and cfg.h are fixed sources
and manually regenerate than whenever cfg.y changes.
