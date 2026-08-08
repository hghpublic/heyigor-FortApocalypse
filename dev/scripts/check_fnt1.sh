python3 -c "
import re
data = open('fnt1.c').read()
# Only count bytes inside the array initializer
array_body = re.search(r'fnt1_data\[128 \* 8\] = \{(.*?)\};', data, re.DOTALL).group(1)
bytes_list = re.findall(r'0x[0-9A-Fa-f]{2}', array_body)
print(f'Array bytes: {len(bytes_list)} (expected 1024)')
"