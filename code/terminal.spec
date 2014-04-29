# -*- mode: python -*-
a = Analysis(['miniterm.py'],
             pathex=['C:\\Users\\Andreas\\TFE4850\\code'],
             hiddenimports=[],
             hookspath=None,
             runtime_hooks=None)
pyz = PYZ(a.pure)
exe = EXE(pyz,
          a.scripts,
          a.binaries,
          a.zipfiles,
          a.datas,
          name='terminal.exe',
          debug=False,
          strip=None,
          upx=True,
          console=True )
