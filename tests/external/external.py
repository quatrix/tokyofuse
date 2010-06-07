import sys
import tc

hdb = tc.HDB()

try:
	hdb.open("casket.tc", tc.HDBOWRITER | tc.HDBOCREAT)
except tc.Error, e:
	sys.stderr.write("open error: %s\n" % (e.args[1]))


try:
	hdb.put("hey", "ho")
	hdb.put("lets", "go")
except tc.Error, e:
	sys.stderr.write("put error: %s\n" % (e.args[1]))
