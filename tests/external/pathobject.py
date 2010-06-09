
class PathObject:
	def __init__(self, fullpath):
		"""
			/baba/012/F10/192
			
			filename  = 192
			fullpath  = /baba/012/F10/192
			fulldir   = /baba/012/F10
			directory = 012/F10
			= 012
		"""
		
		last_slash  = fullpath.rfind('/')

		self.fullpath    = fullpath
		self.filename    = fullpath[last_slash+1:]
		self.parent      = fullpath[:last_slash]
		self.grandparent = self.parent[:self.parent.rfind('/')]


class PathRange:
	def __init__(self, start, stop):
		self.cur = start
		self.start = start
		self.stop = stop

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

		return PathObject(directory + padded[6:])


	def next(self):
		if (self.cur > self.stop):
			raise StopIteration

		else:
			self.cur += 1
			return self.__gen_path(self.cur-1)

class RunOnRange:
	def __init__(self, start, end):
		self.range = PathRange(start, end)

	def run(self, method):
		self.range.rewind()

		for x in self.range:
			method.run(x)
