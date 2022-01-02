NRPROC=9
N=12 #numar procese
BONUS=0

build:
	mpiCC tema3.cpp -o tema3 -lm

run:
	mpirun -oversubscribe -np $(NRPROC) tema3 $(N) $(BONUS)

clean:
	rm tema3
