class PathObject:
	def __init__(self, fullpath):
		"""
			/baba/012/F10/192
			
			prefix    = /baba
			directory = 012/F10
			filename  = 192
			filepath  = 012/F10/192
			fullpath  = /baba/012/F10/192
			fulldir   = /baba/012/F10
			parentdir = 012
		"""
		
		self.fullpath = fullpath
		self.last_slash = self.fullpath.rfind('/')
		self.first_slash = self.fullpath.find('/', 1)
	
	def get_prefix(self):
		return self.fullpath[:self.first_slash]

	def get_filepath(self):
		return self.fullpath[self.first_slash+1:]

	def get_directory(self):
		return self.filepath[:self.filepath.rfind('/')]
		
	def get_filename(self):
		return self.fullpath[self.last_slash+1:]

	def get_parentdir(self):
		return self.directory[:self.directory.find('/')]

	def get_fulldir(self):
		return self.fullpath[:self.last_slash]

	prefix    = property(get_prefix)
	filepath  = property(get_filepath)
	directory = property(get_directory)
	filename  = property(get_filename)
	fulldir   = property(get_fulldir)
	parentdir = property(get_parentdir)


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

		directory = ''

		for i in range(6):
			directory += padded[i]
			
			if i % 3 == 2:
				directory += '/'

		return PathObject(self.prefix + '/' + directory + padded[6:])


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
