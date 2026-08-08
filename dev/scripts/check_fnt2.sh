python3 -c "
import re
data = open('fnt2.c').read()
array_body = re.search(r'fnt2_data\[128 \* 8\] = \{(.*?)\};', data, re.DOTALL).group(1)
count = len(re.findall(r'0x[0-9A-Fa-f]{2}', array_body))
print(f'Array bytes: {count} (expected 1024)')
"