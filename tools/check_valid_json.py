import json
import sys

print(json.loads(open(sys.argv[1]).read()))
