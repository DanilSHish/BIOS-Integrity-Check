import os

SHA256_LEN_IN_BYTES = 32
SHA256_LEN = 48

template_file = open("Temp_hash.txt", 'r')
data_file = open("hash.bin", 'rb')
result_file = open("HashBase.h", 'w')

num_of_hashes = os.stat("hash.bin").st_size

if num_of_hashes % 48 != 0:
	print("Incorrect data! filesize is not multiple to 48.")
	exit()

num_of_hashes = num_of_hashes // SHA256_LEN

for line in template_file:
    result_file.write(line)
result_file.write('[' + str(num_of_hashes) + ']')
result_file.write(' = ')
result_file.write('{\n')

for hash_index in range(num_of_hashes):
	result_file.write('    {\n')
	result_file.write('    	{')
	for i in range(SHA256_LEN_IN_BYTES):
		one_byte = data_file.read(1)
		one_byte = int.from_bytes(one_byte, byteorder='big')
		result_file.write(hex(one_byte))
		if i != SHA256_LEN_IN_BYTES - 1:
			result_file.write(', ')
	result_file.write('},\n')
	result_file.write('    	')
	one_byte = data_file.read(8)
	one_byte = int.from_bytes(one_byte,byteorder='little')
	result_file.write(hex(one_byte))
	result_file.write(',\n')
	result_file.write('    	')
	one_byte = data_file.read(8)
	one_byte = int.from_bytes(one_byte, byteorder='little')
	result_file.write(hex(one_byte))
	if i != SHA256_LEN_IN_BYTES - 1:
		result_file.write(', ')
	result_file.write('\n	},\n')
result_file.write('};\n')

template_file.close()
data_file.close()
result_file.close()
