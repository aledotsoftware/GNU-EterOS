import sys
import re

def search_files(pattern, files):
    for filename in files:
        with open(filename, 'r') as f:
            for line_no, line in enumerate(f, 1):
                if re.search(pattern, line):
                    print(f"{filename}:{line_no}: {line.strip()}")

search_files(sys.argv[1], sys.argv[2:])
