from multiprocessing import Process, Pipe, Event
import random
import sys, os, errno
import tc
import filecmp
import time

def avg(l):
	return sum(l) / float(len(l))

def mid(l):
	return l[len(l) / 2]

class CreateDirectory:
	def __init__(self, prefix):
		self.prefix = prefix + '/'
	def run(self, path):
		try:
			os.makedirs(self.prefix + path.parent)

		except OSError as exc:
			if exc.errno == errno.EEXIST:
				pass
			else:
				raise
		

class CreateRandomFile:
	def __init__(self, prefix, max_size ):
		self.max_size = max_size
		self.prefix = prefix + '/'

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
			self.__create_random_file(self.prefix + path.fullpath)


class CreateTc:
	def __init__(self, file_prefix, tc_prefix):
		self.file_prefix = file_prefix + '/'
		self.tc_prefix= tc_prefix + '/'
		self.tc_files = {}

	def run(self, path):
		tc_file = self.tc_prefix + path.parent + '.tc'
		tc_dir  = self.tc_prefix + path.grandparent
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

			for retry in range(10):
				try:
					hdb.open(tc_file, tc.HDBOWRITER | tc.HDBOCREAT)
					break;
				except tc.Error, e:
					if e[0] == 15:
						# on mmap failure, close all active cabinets
						self.close()
					else:
						sys.stderr.write("open error: %s\n" % (e.args[1]))	
						raise

			self.tc_files[tc_file] = hdb

		try:
			with open(self.file_prefix + path.fullpath, 'rb') as f:
				hdb.put(tc_key, f.read())

		except tc.Error, e:
			sys.stderr.write("put error: %s\n" % (e.args[1]))
			raise

	def close(self):
		for hdb in self.tc_files.itervalues():
			hdb.close()

		self.tc_files.clear()



class DiffError(Exception):
	def __init__(self, value):
		self.value = value

	def __str__(self):
		return repr(self.value)


class Diff:
	def __init__(self, file_prefix, tc_prefix):
		self.file_prefix = file_prefix + '/'
		self.tc_prefix   = tc_prefix + '/'
		self.directories    = {}

	def run(self, path):
		tc_file = self.tc_prefix + path.fullpath
		or_file = self.file_prefix + path.fullpath


		#if filecmp.cmp(tc_file, or_file, shallow = False) == False:
		#	raise DiffError("%s != %s" % (tc_file, or_file))

		try:
			self.directories[path.parent].append(path.filename)
		except KeyError:
			self.directories[path.parent] = [path.filename]


	def scale_test(self, forks):
		def __bench_diff(start_now, conn, files):
			start_now.wait()

			t_res = []

			for file in files:
				t0 = time.time()

				#print "%s %s" % (file[0], file[1])
				if filecmp.cmp(file[0], file[1], shallow = False) == False:
					raise DiffError("%s != %s" % (file[0], file[1]))

				t_res.append((time.time() - t0) * 1000.0)

			t_res.sort()
			
			conn.send((t_res[0], t_res[-1], avg(t_res), mid(t_res)))
			conn.close()


		for n_forks in range(1, 1000, 10):
			workers = []
			start_now = Event()

			print "starting %d concurrent forks" % (n_forks)

			for forks in range(n_forks):
				random_files = []

				while len(random_files) < 5:
					directory = random.choice(self.directories.keys())
					file = random.choice(self.directories[directory])

					tc_file= self.tc_prefix + directory + '/' + file
					orig_file = self.file_prefix + directory + '/' + file

					random_files.append((tc_file, orig_file))

				parent_conn, child_conn = Pipe()
				p = Process(target=__bench_diff, args=(start_now, child_conn, random_files))
				p.start()

				workers.append((p, parent_conn))

			print "all forks up, setting start_now"	
			start_now.set()
			print "results for %d concurrent forks:" % (n_forks)


			avg_min = []
			avg_max = []
			avg_avg = []
			avg_mid = []

			for p in workers:
				res = p[1].recv()
				avg_min.append(res[0])
				avg_max.append(res[1])
				avg_avg.append(res[2])
				avg_mid.append(res[3])

				p[0].join()


			print "min: %0.2f (%0.2f) max: %0.2f (%0.2f) avg: %0.2f mid: %0.2f" % (min(avg_min), avg(avg_min),  max(avg_max), avg(avg_max), avg(avg_avg), avg(avg_mid))
			print "--------------------"


	def dir_diff(self, forks):
		workers = []

		def __dir_diff(conn, dir_a, dir_b, files):
			print "starting dir diff between %s and %s" % (dir_a, dir_b)

			diff = []

			for file in files:
				file_a = dir_a + '/' + file
				file_b = dir_b + '/' + file
				#t0 = time.time()
				if filecmp.cmp(file_a, file_b, shallow = False) == False:
					diff.append(file)
				#td = (time.time() - t0) * 1000.0

				"""
				if td > 300:	
					print "%10d |" % td
				else:
					print "%10s | %d" % ('', td)
				"""




			#d = filecmp.cmpfiles(dir_a, dir_b, files, shallow=False)

			#diff = d[1]
			#diff.extend(d[2])



			conn.send(diff)

			#d = filecmp.dircmp(dir_a, dir_b)

			#diff = []
			#diff.extend(d.left_only)
			#diff.extend(d.right_only)
			#diff.extend(d.diff_files)

			#conn.send(diff)
			conn.close()

		for directory, files in self.directories.items():
			tc_dir   = self.tc_prefix + directory
			file_dir = self.file_prefix + directory

			for i in range(forks):
				parent_conn, child_conn = Pipe()
				p = Process(target=__dir_diff, args=(child_conn, file_dir, tc_dir, files ))
				p.start()

				workers.append((p, parent_conn))

		for p in workers:
			res = p[1].recv()

			if (res):
				raise DiffError(res)

			p[0].join()

