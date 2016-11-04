ZJS Dependencies
================

This directory contains projects that the ZJS work depends on so we can
maintain our own patches to them when necessary.

Now we use git submodules to maintain these dependencies, so the old scripts
that were here have been removed. Use `make update` to sync the dependencies
to the commits they are supposed to be on given the version of ZJS you have
checked out. This update target runs automatically when you build normally.

To update the commit for a dependency directory, change to the `deps`
subdirectory and check out the desired commit. Then when you look at your
`git diff` back at the root level, you will see the change and can commit it.

For example:

```bash
$ cd deps/zephyr
$ git checkout v1.6.0
$ cd ../..
$ git add deps/zephyr
$ git commit
```
