# Coding Agent Overseer

This is a general-purpose "coding agent overseer" initializer.

If you are reading this file, it probably means you have just downloaded this repo and/or are lost.

This is intended to be used with a CLI coding agent in a multi-repo/branch environment. It contains very little in the way of end-user documentation.

This is intended to be installed alongside other branches/repos when you need a coding agent that understands the nuances of multi-repo environments.

Your filesystem layout should look something like this:

- <some-parent-directory>/
  - README.md (copy this repo's `install/README.parent.md` and replace `<overseer-directory>` with this repo's directory name)
  - <some-repo>/
  - <some-other-repo>/
  - <however-many-repos-you-want>/
  - <this-repo>/

If your filesystem does not already look like that, then this repo is "dangling". It cannot function unless the installation structure is as shown above.

If your filesystem already looks like the one shown above, then:
- Go into <some-parent-directory>
- Follow the instructions in README.md

If your filesystem does not already look like that, and you have no idea how to fix this problem and/or you have no idea what is being talked about here, it is unlikely you are going to be able to make meaningful progress on this project.
