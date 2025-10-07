import ctypes
import os
import sys

def find_library():
	# Try a list of common names depending on platform
	root = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
	candidates = []
	if sys.platform == 'win32':
		candidates = ['universal_multi_segmented_bi_buffer_bus.dll', 'libuniversal_multi_segmented_bi_buffer_bus.dll', 'universal_multi_segmented_bi_buffer_bus.so']
	elif sys.platform == 'darwin':
		candidates = ['libuniversal_multi_segmented_bi_buffer_bus.dylib', 'libuniversal_multi_segmented_bi_buffer_bus.so']
	else:
		candidates = ['libuniversal_multi_segmented_bi_buffer_bus.so', 'universal_multi_segmented_bi_buffer_bus.so', 'universal_multi_segmented_bi_buffer_bus.dll']

	for name in candidates:
		path = os.path.join(root, name)
		if os.path.exists(path):
			return path

	# Also try common CMake output folders (Release/Debug and x64 variants)
	build_dirs = [os.path.join(root, 'build'),
				  os.path.join(root, 'build', 'Release'),
				  os.path.join(root, 'build', 'Debug'),
				  os.path.join(root, 'build', 'x64', 'Release'),
				  os.path.join(root, 'build', 'x64', 'Debug')]

	for d in build_dirs:
		for name in candidates:
			p = os.path.join(d, name)
			if os.path.exists(p):
				return p

	return None


libpath = find_library()
if not libpath:
	sys.stderr.write("Could not find a built shared library (libuniversal_multi_segmented_bi_buffer_bus).\n")
	sys.stderr.write("Build a shared library with CMake before running this script.\n")
	sys.stderr.write("Example (PowerShell):\n")
	sys.stderr.write("  mkdir build; cd build; cmake ..; cmake --build . --config Release\n")
	sys.exit(1)

print(f"Found library: {libpath}")
try:
	lib = ctypes.CDLL(libpath)
	print("Library loaded via ctypes.CDLL()")
except Exception as e:
	print("Failed to load library:", e)
	sys.exit(1)

try:
	lib.umsbb_init.restype = ctypes.c_void_p
	lib.umsbb_submit.argtypes = [ctypes.c_void_p, ctypes.c_char_p, ctypes.c_size_t]
	lib.umsbb_drain.argtypes = [ctypes.c_void_p]
	lib.umsbb_free.argtypes = [ctypes.c_void_p]
	print("Function prototypes set")
except Exception as e:
	print("Failed to set function prototypes:", e)
	sys.exit(1)

try:
	bus = lib.umsbb_init(1024, 2048)
	print("umsbb_init returned", bus)
except Exception as e:
	print("umsbb_init failed:", e)
	sys.exit(1)

try:
	lib.umsbb_submit(bus, b"Capsule from Python", 20)
	print("umsbb_submit OK")
except Exception as e:
	print("umsbb_submit failed:", e)
	sys.exit(1)

try:
	lib.umsbb_drain(bus)
	print("umsbb_drain OK")
except Exception as e:
	print("umsbb_drain failed:", e)
	sys.exit(1)

try:
	lib.umsbb_free(bus)
	print("umsbb_free OK")
except Exception as e:
	print("umsbb_free failed:", e)
	sys.exit(1)