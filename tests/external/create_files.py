import random
import os, errno
import tc

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
	
	def run(self, path_object):
			self.__create_random_file(path_object.filename())

class CreateDirectory:
	def run(self, path_object):
		try:
			os.makedirs(path_object.directory())

		except OSError as exc:
			if exc.errno == errno.EEXIST:
				pass
			else:
				raise
		
class PathObject:
	def __init__(self, prefix, dir, file):
		self.prefix = prefix
		self.dir = dir
		self.file = file

	def filename(self):
		return self.prefix + '/' + self.file

	def directory(self):
		return self.prefix + '/' + self.dir

class PathRange:
	def __init__(self, prefix, start, stop):
		self.cur = start
		self.start = start
		self.stop = stop
		self.prefix = prefix

	def rewind(self):
		self.cur = self.start

	def __iter__(self):
		return self

	def __gen_path(self, id):
		padded = ("%09x") % id

		dir = ''

		for i in range(6):
			dir += padded[i]
			
			if i % 3 == 2:
				dir += '/'

		file = dir + padded[6:]
		dir  =  dir


		return PathObject(self.prefix, dir, file)

	def next(self):
		if (self.cur > self.stop):
			raise StopIteration

		else:
			self.cur += 1
			return self.__gen_path(self.cur-1)
		

class RunOnRange:
	def __init__(self, prefix, start, end):
		self.range = PathRange(prefix, start, end)

	def run(self, method):
		self.range.rewind()

		for x in self.range:
			method.run(x)


#x = RandomFile(max_size = 1024)

#x.create_random_file("/tmp/baba.txt")

class CreateTc:
	def __init__(self, prefix):
		self.prefix = prefix
		self.tc_files = {}

	def run(self, x):
		tc_file = self.prefix + '/' + x.dir[:x.dir.rfind('/')] + '.tc'
		tc_dir = self.prefix + '/' + x.dir[:x.dir.find('/')]
		tc_key = x.file[x.file.rfind('/')+1:]

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
			with open(x.filename(), 'rb') as f:
				hdb.put(tc_key, f.read())

		except tc.Error, e:
			sys.stderr.write("put error: %s\n" % (e.args[1]))
			raise

	def close(self):
		for hdb in self.tc_files.itervalues():
			hdb.close()
				




tcCreator = CreateTc('/baba')

x = RunOnRange('/pita', 0, 5000)

#x.run(CreateDirectory())
#x.run(RandomFile(max_size = 1024))
x.run(tcCreator)
tcCreator.close()
