import os
import glob


inkscape =  r"C:\Program Files\Inkscape\inkscape.exe"

#this one supports matlab's bounding boxes:
epstopdf =  r"C:\Program Files\MiKTeX 2.7\miktex\bin\epstopdf.exe"

curdir = os.path.realpath( os.path.curdir )


def isNewer(fout, fin):
        """check if fout needs to be updated"""
        if os.path.isfile(fin) and os.path.isfile(fout):
                return os.path.getmtime(fin) > os.path.getmtime(fout)
        else:
                return True


for fname in glob.glob("*.svg"):
        base , ext = os.path.splitext(fname)
        png = base + '.png'

        src = os.path.join(curdir,fname)
        dst = os.path.join(curdir,png)

        if isNewer(dst,src):
                cmd = '"%s" -f %s -e %s' % (inkscape , src, dst)
                print cmd
                os.system(cmd)

raw_input('press enter')
