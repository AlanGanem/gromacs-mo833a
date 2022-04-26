import subprocess
from tqdm import tqdm
from pathlib import Path
from typer import Typer
import sys

cli = Typer()

def run_command(cmd, **popen_kwargs):
    with subprocess.Popen(cmd, stdout=subprocess.PIPE,stderr=subprocess.PIPE, bufsize=1, universal_newlines=True, **popen_kwargs) as p:
        for line in p.stdout:
            print(line, end='') # process line here

    if p.returncode != 0:
        raise CalledProcessError(p.returncode, p.args)

    return

def _build_debug_exp1():
	command = 'cd ../.. ; mkdir -p debug; cd debug ; cmake ../.. -DGMX_BUILD_OWN_FFTW=ON -DCMAKE_BUILD_TYPE=Debug ; make'
	run_command(command, shell = True)
	return
	
def _build_release_exp1():
	command = 'cd ../.. ; mkdir -p release; cd release ; cmake ../.. -DGMX_BUILD_OWN_FFTW=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-pg -DCMAKE_EXE_LINKER_FLAGS=-pg -DCMAKE_SHARED_LINKER_FLAGS=-pg ; make'
	run_command(command, shell = True)
	return
	
@cli.command()
def build_experiment1():
	path = Path('.')	

	if not Path('../6LVN.pdb').exists():
		subprocess.run('cd .. ; wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/6LVN.pdb', shell = True)
	if not Path('../ions.mdp').exists():
		subprocess.run('cd .. ; wget https://www.ic.unicamp.br/~edson/disciplinas/mo833/2021-1s/anexos/ions.mdp', shell = True)

	_build_release_exp1()
	return
		
@cli.command()
def run_experiment1(profiler:str, simulation_only:bool = False, experiments_folder:str = '..'):
    '''
    Parameters
    ----------
    profiler: {'gprof', 'perf'}
        profiler to use
    simulation_only: bool
        whether to skip simulation setup
    experiments_folder: Path
        str of path of the experimetns folder

    Returns
    -------
    None
    '''
    COMMANDS = [
    	'cd ..',
    	rf"##BINPATH## pdb2gmx -f 6LVN.pdb -o 6LVN_processed.gro -water spce -ff oplsaa",
    	rf"##BINPATH## editconf -f 6LVN_processed.gro -o 6LVN_newbox.gro -c -d 1.0 -bt cubic",
    	rf"##BINPATH## solvate -cp 6LVN_newbox.gro -cs spc216.gro -o 6LVN_solv.gro -p topol.top",
	    rf"##BINPATH## grompp -f ions.mdp -c 6LVN_solv.gro -p topol.top -o ions.tpr",
    	rf"##BINPATH## genion -s ions.tpr -o 6LVN_solv_ions.gro -p topol.top -pname NA -nname CL -neutral",
    	rf"##BINPATH## grompp -f ions.mdp -c 6LVN_solv_ions.gro -p topol.top -o em.tpr",    	
    ]
    
    if profiler == 'perf':
        COMMANDS.append(rf"perf record -g ##BINPATH## mdrun -v -deffnm em")
    elif profiler == 'gprof':
        COMMANDS.append(rf'gprof ##BINPATH## mdrun -v -deffnm em')
    else:
        raise ValueError(rf'profiler should be one of ["perf","gprof"], got {profiler}')
    #command calls    
    setup_command = '; '.join([i.replace('##BINPATH##', rf'{experiments_folder}/release/bin/gmx') for i in COMMANDS[:-1]])
    #setup
    if not simulation_only:
        subprocess.run(setup_command, shell=True, input = b'13')
    #define final command (run simulation)
    final_command = 'cd .. ; ' + COMMANDS[-1].replace('##BINPATH##', rf'{experiments_folder}/release/bin/gmx')
    run_command(final_command, shell=True)
    return
	
if __name__ == '__main__':
	cli()	
	


