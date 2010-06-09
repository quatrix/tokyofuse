import unittest
from pathobject import RunOnRange, PathObject
from pathmodules import CreateDirectory, CreateTc,  CreateRandomFile, Diff, DiffError
import os

RANGE_START    = 99999999
RANGE_STOP     = RANGE_START + 8096
FILE_PREFIX    = '/tmp/tctest/files'
DIFF_FORKS     = 10
MAX_FILESIZE   = 1024 * 5
TC_PREFIX      = '/tmp/tctest/tc'
TC_FUSE_MOUNT  = '/tmp/tcfuse'
TC_FUSE_BINARY = '../../src/tokyofuse'
TC_CREATED     = '/tmp/tc.created'


class TokyoFuseTest(unittest.TestCase):
	range = RunOnRange( RANGE_START, RANGE_STOP )
	diff = Diff(FILE_PREFIX, TC_FUSE_MOUNT)

	def setup_workdirs(self):
		if os.path.exists(TC_CREATED):
			return

		self.range.run(CreateDirectory(FILE_PREFIX))
		self.range.run(CreateRandomFile(FILE_PREFIX, MAX_FILESIZE))

		tc_creator = CreateTc(FILE_PREFIX, TC_PREFIX)
		self.range.run(tc_creator)

		tc_creator.close()

		f = open(TC_CREATED, "w")
		f.close()

	def setUp(self):
		self.setup_workdirs()

		os.system("fusermount -u %s &>/dev/null" % TC_FUSE_MOUNT)
		self.assertFalse(os.system("mkdir -p %s" % TC_FUSE_MOUNT))
		self.assertFalse(os.system("%s -o modules=subdir,subdir=%s %s" % (TC_FUSE_BINARY, TC_PREFIX, TC_FUSE_MOUNT)))

	def tearDown(self):
		self.assertFalse(os.system("fusermount -u %s" % TC_FUSE_MOUNT))


	def test_diff(self):
		try:
			print "running seq diff test"
			self.range.run(self.diff)

			print "running forked diff test"
			self.diff.dir_diff(DIFF_FORKS) 
		except DiffError, e:
			self.fail(e)
			
if __name__ == '__main__':
	unittest.main()
