from pathobject import RunOnRange, PathObject
import random
import os, errno
import tc


class CreateDirectory:
	def run(self, path):
		try:
			os.makedirs(path.fulldir)

		except OSError as exc:
			if exc.errno == errno.EEXIST:
				pass
			else:
				raise
		

class RandomFile:
	def __init__(self, max_size ):
		self.max_size = max_size

	def __random_char(self):
		return chr(random.randrange(255))

	def __random_content(self):
		r = ''

		for i in range(random.randrange(self.max_size-1)):
			r += self.__random_char()

		return r

	def __create_random_file(self, filename):
		with open(filename, 'wb') as f:
			f.write(self.__random_content())
	
	def run(self, path):
			self.__create_random_file(path.fullpath)


class CreateTc:
	def __init__(self, prefix):
		self.prefix = prefix
		self.tc_files = {}

	def run(self, path):
		tc_file = self.prefix + '/' + path.directory + '.tc'
		tc_dir  = self.prefix + '/' + path.parentdir
		tc_key  = path.filename

		try:
			hdb = self.tc_files[tc_file]
		except KeyError:
			hdb = tc.HDB()

			try:
				os.makedirs(tc_dir)
			except OSError as exc:
				if exc.errno == errno.EEXIST:
					pass
				else:
					raise
			try:
				hdb.open(tc_file, tc.HDBOWRITER | tc.HDBOCREAT)
			except tc.Error, e:
				 sys.stderr.write("open error: %s\n" % (e.args[1]))	
				 raise

			self.tc_files[tc_file] = hdb


		try:
			with open(path.fullpath, 'rb') as f:
				hdb.put(tc_key, f.read())

		except tc.Error, e:
			sys.stderr.write("put error: %s\n" % (e.args[1]))
			raise

	def close(self):
		for hdb in self.tc_files.itervalues():
			hdb.close()
				


x = RunOnRange( "/baba", 99999999, 99999999+8096)

x.run(CreateDirectory())
x.run(RandomFile(max_size = 1024))
x.run(CreateTc('/pita'))
