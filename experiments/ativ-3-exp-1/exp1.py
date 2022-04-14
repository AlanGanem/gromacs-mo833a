import subprocess
from tqdm import tqdm
from pathlib import Path
from typer import Typer

cli = Typer()


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
	
@cli.command()
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

	if not Path('./6LVN.pdb').exists():
		subprocess.run('wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/6LVN.pdb', shell = True)
	if not Path('./ions.mdp').exists():
		subprocess.run('wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/ions.mdp', shell = True)
	return
		
@cli.command()
def run_experiment1(nruns:int = 10, mode:str = 'all', experiments_folder:str = '..', write_files:bool = True):
    '''
    Parameters
    ----------
    nruns: bool
        n times to run experiment

    mode: {'debug','release','all'}
        compilation mode to run experiment with
    
    experiments_folder: Path
        str of path of the experimetns folder
    
    write_files: bool
        whether to write output as csv file
        
    Returns
    -------
    list containing time elapsed in seconds
    '''
    #recursive call if mode == 'all'
    if mode == 'all':
        release = run_experiment1(nruns = nruns, mode = 'release', experiments_folder = experiments_folder, write_files = write_files)
        debug = run_experiment1(nruns = nruns, mode = 'debug', experiments_folder = experiments_folder, write_files = write_files)        
        return release, debug
    
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
    setup_command = '; '.join([i.replace('##BINPATH##', rf'{experiments_folder}/{mode}/bin/gmx') for i in COMMANDS[:-1]])
    #setup
    subprocess.run(setup_command, capture_output=True, shell=True, input = b'13')
    #define final command (run simulation)
    final_command = COMMANDS[-1].replace('##BINPATH##', rf'{experiments_folder}/{mode}/bin/gmx')
    pbar = tqdm(list(range(nruns)))
    pbar.set_description(f"Running {mode} mode experiments: ")
    for i in pbar:
        result = subprocess.run(final_command, capture_output=True, shell=True)
        s = result.stdout.decode('utf-8')
        sub1 = 'runner.mdrunner() exec. time: '
        sub2 = ' !'
        a, b = s.find(sub1), s.find(sub2)
        time = s[a+len(sub1):b]
        results.append(time)
        
    if write_files:
        results = ",".join(results)
        with open(f'{mode}_run_data.csv','w') as f:
            f.write(results)
        print(f"{mode} execution time data exported to {Path('debug_run_data.csv').resolve()}")
    return results
	
if __name__ == '__main__':
	cli()	
	


