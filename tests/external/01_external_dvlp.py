from pathobject  import RunOnRange, PathObject
from pathmodules import CreateDirectory, CreateTc,  CreateRandomFile, Diff, DiffError
from optparse    import OptionParser
import os, sys,  unittest

opts = OptionParser()

opts.add_option("--tc_fuse_binary", type="string", dest="tc_fuse_binary", default="tokyofuse" )
opts.add_option("--file_prefix",    type="string", dest="file_prefix",    default="/tmp/tctest/files" )
opts.add_option("--tc_prefix",      type="string", dest="tc_prefix",      default="/tmp/tctest/tc"    )
opts.add_option("--diff_forks",     type="int",    dest="diff_forks",     default=10                  )
opts.add_option("--tc_mount",       type="string", dest="tc_fuse_mount",  default="/tmp/tcfuse"       )
opts.add_option("--tc_created",     type="string", dest="tc_created",     default="/tmp/tc.created"   )
opts.add_option("--num_of_files",   type="int",    dest="num_of_files",   default=1024*8              )
opts.add_option("--max_filesize",   type="int",    dest="max_filessize",  default=1024*5              )

(options, args) = opts.parse_args(sys.argv)

RANGE_START    = 0
RANGE_STOP     = RANGE_START + options.num_of_files
DIFF_FORKS     = options.diff_forks
MAX_FILESIZE   = options.max_filessize
FILE_PREFIX    = options.file_prefix
TC_PREFIX      = options.tc_prefix
TC_FUSE_MOUNT  = options.tc_fuse_mount
TC_FUSE_BINARY = options.tc_fuse_binary
TC_CREATED     = options.tc_created


class TokyoFuseTest(unittest.TestCase):
	range = RunOnRange( RANGE_START, RANGE_STOP )
	diff = Diff(FILE_PREFIX, TC_FUSE_MOUNT)

	def setup_workdirs(self):
		if os.path.exists(TC_CREATED):
			return

		print "creating directories"
		self.range.run(CreateDirectory(FILE_PREFIX))

		print "creating files"
		self.range.run(CreateRandomFile(FILE_PREFIX, MAX_FILESIZE))

		print "creating cabinets"
		tc_creator = CreateTc(FILE_PREFIX, TC_PREFIX)
		self.range.run(tc_creator)

		tc_creator.close()

		f = open(TC_CREATED, "w")
		f.close()

	def setUp(self):
		print "%s -o modules=subdir,subdir=%s %s" % (TC_FUSE_BINARY, TC_PREFIX, TC_FUSE_MOUNT)
		self.setup_workdirs()

		#os.system("fusermount -u %s &>/dev/null" % TC_FUSE_MOUNT)
		#self.assertFalse(os.system("mkdir -p %s" % TC_FUSE_MOUNT))
		#self.assertFalse(os.system("%s -o modules=subdir,subdir=%s %s" % (TC_FUSE_BINARY, TC_PREFIX, TC_FUSE_MOUNT)))

	def tearDown(self):
		return
		self.assertFalse(os.system("fusermount -u %s" % TC_FUSE_MOUNT))


	def test_diff(self):
		try:
			print "running seq diff test"
			self.range.run(self.diff)

			print "running forked diff test"
			self.diff.scale_test(DIFF_FORKS) 
		except DiffError, e:
			self.fail(e)
			
if __name__ == '__main__':


	suite = unittest.TestLoader().loadTestsFromTestCase(TokyoFuseTest)
	unittest.TextTestRunner(verbosity=0).run(suite)

	#unittest.main()

