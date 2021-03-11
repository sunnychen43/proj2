import os
import subprocess

def execute(cmd):
    popen = subprocess.Popen(cmd, stdout=subprocess.PIPE, universal_newlines=True)
    for stdout_line in iter(popen.stdout.readline, ""):
        yield stdout_line 
    popen.stdout.close()
    return_code = popen.wait()
    if return_code:
        raise subprocess.CalledProcessError(return_code, cmd)

subprocess.run(['make', 'clean'])
subprocess.run(['make'])

os.chdir('benchmarks')

subprocess.run(['make', 'clean'])
subprocess.run(['make'])
subprocess.run(['./genRecord.sh'])

processes = 50

external_cal_test = subprocess.Popen(['./external_cal', str(processes)], stdout=subprocess.PIPE)
parallel_cal_test = subprocess.Popen(['./parallel_cal', str(processes)], stdout=subprocess.PIPE)
vector_multiply_test = subprocess.Popen(['./vector_multiply', str(processes)], stdout=subprocess.PIPE)
# test = subprocess.Popen(['./test', str(processes)], stdout=subprocess.PIPE)

print('')
print('./vector_multiply')
print(vector_multiply_test.communicate()[0].decode())

print('./external_cal')
print(external_cal_test.communicate()[0].decode())

print('./parallel_cal')
print(parallel_cal_test.communicate()[0].decode())

# print('./test') 
# print(test.communicate()[0].decode())