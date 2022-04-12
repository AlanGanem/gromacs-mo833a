import subprocess
from tqdm import tqdm
from pathlib import Path

def _build_debug_exp1():
	command = 'cd .. ; mkdir -p debug; cmake ../.. -DGMX_BUILD_OWN_FFTW=ON -DCMAKE_BUILD_TYPE=Debug ; make'
	sub = subprocess.run(command, capture_output=True, shell=True)
	print(sub.stdout.decode())	
	return
	
def _build_release_exp1():
	command = 'cd .. ; mkdir -p release; cmake ../.. -DGMX_BUILD_OWN_FFTW=ON -DCMAKE_BUILD_TYPE=Release ; make'
	sub = subprocess.run(command, capture_output=True, shell=True)
	print(sub.stdout.decode())
	return
	
def build_experiment1():
	path = Path('.')
	if (not (path/'../debug').exists()) or (not list(((path/'../debug')).rglob('bin/gmx'))):
		_build_debug_exp1()
	else:
		print(f"Debug version already built. Binary under {list(((path/'../debug')).rglob('bin/gmx'))[0].resolve()}")
	
	if (not (path/'../release').exists()) or (not list(((path/'../release')).rglob('bin/gmx'))):
		_build_release_exp1()
	else:
		print(f"Release version already built. Binary under {list(((path/'../release')).rglob('bin/gmx'))[0].resolve()}")
	
	return
		
def run_experiment1(nruns = 10, mode = 'debug', project_folder = '..'):
    
    COMMANDS = [
    	rf"##BINPATH## pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce -ff oplsaa",
    	rf"##BINPATH## editconf -f 6LVN_processed.gro -o 6LVN_newbox.gro -c -d 1.0 -bt cubic",
    	rf"##BINPATH## solvate -cp 6LVN_newbox.gro -cs spc216.gro -o 6LVN_solv.gro -p topol.top",
	    rf"##BINPATH## grompp -f ions.mdp -c 6LVN_solv.gro -p topol.top -o ions.tpr",
    	rf"##BINPATH## genion -s ions.tpr -o 6LVN_solv_ions.gro -p topol.top -pname NA -nname CL -neutral",
    	rf"##BINPATH## grompp -f ions.mdp -c 6LVN_solv_ions.gro -p topol.top -o em.tpr",
    	rf"##BINPATH## mdrun -v -deffnm em"
    ]
    #command calls
    results = []
    setup_command = '; '.join([i.replace('##BINPATH##', rf'{project_folder}/{mode}/bin/gmx') for i in COMMANDS[:-1]])
    #setup
    subprocess.run(setup_command, capture_output=True, shell=True, input = b'13')
    #define final command (run simulation)
    final_command = COMMANDS[-1].replace('##BINPATH##', rf'{project_folder}/{mode}/bin/gmx')
    for i in tqdm(list(range(nruns))):
        result = subprocess.run(final_command, capture_output=True, shell=True)
        s = result.stdout.decode('utf-8')
        sub1 = 'runner.mdrunner() exec. time: '
        sub2 = ' !'
        a, b = s.find(sub1), s.find(sub2)
        time = float(s[a+len(sub1):b])
        results.append(time)        
    
    return results
    
if __name__ == '__main__':
	
	build_experiment1()
	debug = run_experiment1(nruns = 10, mode = 'debug', project_folder = '..')
	release = run_experiment1(nruns = 10, mode = 'release', project_folder = '..')
	debug = ",".join(debug)
	release = ",".join(release)
	with open('debug_run_data.csv','w') as f:
		f.write(debug)
	with open('release_run_data.csv','w') as f:
		f.write(release)
	
	


