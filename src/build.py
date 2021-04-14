#! python2
import os
import sys
import shutil

all_targets = {
    'debug_x86': 'output\\Debug',
    'debug_x64': 'output\\Debug_x64',
    'release_x86': 'output\Release',
    'release_x64': 'output\\Release_x64',
}


def match_one(target, pattern):
  l1 = len(target)
  l2 = len(pattern)
  i = 0
  j = 0
  x = -1
  y = -1
  while i < l1:
    if j < l2 and target[i] == pattern[j]:
      i, j = i + 1, j + 1
    elif j < l2 and pattern[j] == '*':
      x, y, j = i, j, j + 1
    elif x >= 0:
      x, i, j = x + 1, x + 1, y + 1
    else:
      return False
  while j < l2 and pattern[j] == '*':
    j = j + 1

  return j == l2


def match(target, patterns):
  for pattern in patterns:
    if target == pattern or match_one(target, pattern):
      return True
  return False


def main(argv):
  patterns = ['release_x86']
  if len(argv) >= 2:
    patterns = argv[1:]

  is_first = True
  for target in sorted(all_targets.keys()):
    if match(target, patterns):
      if is_first:
        is_first = False
      else:
        print
      print 'Build %s' % target
      exit_code = os.system('ninja -C %s' % all_targets[target])
      if exit_code != 0:
        print 'Failed to build %s' % target
        sys.exit(exit_code)


if __name__ == '__main__':
  main(sys.argv)
