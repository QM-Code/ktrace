# Syntax:
# - One mapping per line: name: value
# - Required root line: root: /absolute/path/to/multi-repo-parent
# - Other entries map directory name -> fetch/clone command
# - Blank lines and lines starting with # are allowed
#
# Example:
# root: /home/user/dev/my-multi-repo-root
# m-overseer: git clone --branch m-overseer https://github.com/example/project.git m-overseer
# m-rewrite: git clone --branch m-rewrite https://github.com/example/project.git m-rewrite
