// Dumitrescu Andrei 333CC
#include <iostream>
#include <fstream>
#include "mpi.h"
#include <map>
#include <vector>
#include <string>

using namespace std;

// Read information for each coordinator
void readChildren(string filename, int rank,
                    vector<int>& innerCluster,
                    map<int, int>& parent) {
    ifstream f(filename);
    int N;

    f >> N;

    for (int i = 0; i < N; ++i) {
        int child;

        f >> child;
        innerCluster.push_back(child);
        parent[child] = rank;
    }

    f.close();
}

// Send parent and topology to each child from the cluster
void sendToChildren(vector<int> innerConnection,
                    int rank,
                    vector<pair<int,int>> data) {
    int sz = data.size();

    for (auto it : innerConnection) {
        MPI_Send(&rank, 1, MPI_INT, it, 0, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
        MPI_Send(&sz, 1, MPI_INT, it, rank, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
        MPI_Send((void*)data.data(), sz * sizeof(pair<int, int>), MPI_BYTE, it, rank, MPI_COMM_WORLD);
        cout << "M(" << rank << "," << it << ")\n";
    }
}

// Print topology in each process.
void printSystem(int rank, vector<pair<int, int>> elements) {
    string ans = to_string(rank) + " -> ";

    for (int i = 0; i < 3; ++i) {
        ans.push_back((i + '0'));
        ans.push_back(':');

        for (auto it : elements) {
            if (it.second == i) {
                ans += to_string(it.first) + ",";
            }
        }

        ans.pop_back();
        ans.push_back(' ');
    }

    ans.pop_back();

    cout << ans << '\n';
}

// Second cluster will get each topology from cluster 0 and 1.
// It will build the final topology and will send it back to each coordinator
// and to its workers.
void secondClusterCoordConn(map<int, int>& parent, vector<int>& innerCone) {
    int numberOfElements;
    MPI_Status status;
    vector<int> elements;

    // Information from the first Cluster.
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 0;
        }
    }
    
    // Information from the thirs Cluster.
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 1, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 1;
        }
    }

    // Store topology into a vector of pairs.
    vector<pair<int, int>> mapPairs;
    int sz;

    for (auto it : parent) {
        mapPairs.push_back({it.first, it.second});
    }

    printSystem(2, mapPairs);
    sz = mapPairs.size();

    { // Send to the other coordinators.
        MPI_Send(&sz, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
        cout << "M(" << 2 << "," << 0 << ")\n";
        MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
        cout << "M(" << 2 << "," << 0 << ")\n";

        MPI_Send(&sz, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
        cout << "M(" << 2 << "," << 1 << ")\n";
        MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
        cout << "M(" << 2 << "," << 1 << ")\n";
    }
    

    sendToChildren(innerCone, 2, mapPairs);
}

// Send topology to cluster 2 and then receive the final topology.
// Send topolofy to workers.
void clusterConn(map<int, int>& parent, vector<int> innerCluster, int rank) {
    int numberOfElements = innerCluster.size();
    vector<pair<int, int>> elements;
    MPI_Status status;

    MPI_Send(&numberOfElements, 1, MPI_INT, 2, 0, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << 2 << ")\n";
    MPI_Send((void*)innerCluster.data(), numberOfElements * sizeof(int), MPI_BYTE, 2, 0, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << 2 << ")\n";

    MPI_Recv(&numberOfElements, 1, MPI_INT, 2, 0, MPI_COMM_WORLD, &status);
    elements = vector<pair<int,int>>(numberOfElements);
    MPI_Recv((void*)elements.data(), numberOfElements * sizeof(pair<int,int>), MPI_BYTE, 2, 0, MPI_COMM_WORLD, &status);

    for (auto it : elements) {
        parent[it.first] = it.second;
    }

    printSystem(rank, elements);
    sendToChildren(innerCluster, rank, elements);
}

// Build topology for workers.
void getSystemForChild(int rank, map<int, int>& parent, int& ancestor) {
    vector<pair<int, int>> elements;
    MPI_Status status;
    int numberOfElements;

    MPI_Recv(&ancestor, 1, MPI_INT, MPI_ANY_SOURCE, 0, MPI_COMM_WORLD, &status);
    MPI_Recv(&numberOfElements, 1, MPI_INT, ancestor, ancestor, MPI_COMM_WORLD, &status);
    elements = vector<pair<int,int>>(numberOfElements);
    MPI_Recv((void*)elements.data(), numberOfElements * sizeof(pair<int,int>), MPI_BYTE, ancestor, ancestor, MPI_COMM_WORLD, &status);

    for (auto it : elements) {
        parent[it.first] = it.second;
    }

    printSystem(rank, elements);
}

// Find number of workers for each cluster.
int numberOfChildren(int rank, map<int, int> mp) {
    int ans = 0;

    for (auto it : mp) {
        if (it.second == rank) {
            ans++;
        }
    }

    return ans;
}

// Calculate load for each worker.
// Calcualte load for cluster 0.
// Send the rest of the work to the other clusters.
// Calcualte load for each worker and send the part of the array
// to them.
// Receive the array from the workers and build the final array.
void processLoad(map<int, int> parent,
                vector<int>& arr,
                int arrSize,
                vector<int> innerCluster) {
    int workers = parent.size() - 3;
    int load = arrSize / workers;
    int firstClusterLoad = min(load * numberOfChildren(0, parent), arrSize);
    int remained = arrSize - firstClusterLoad;
    MPI_Status status;
    int nrKids = numberOfChildren(0, parent);

    MPI_Send(&load, 1, MPI_INT, 2, 4, MPI_COMM_WORLD);
    cout << "M(0,2)\n";
    MPI_Send(&remained, 1, MPI_INT, 2, 4, MPI_COMM_WORLD);
    cout << "M(0,2)\n";
    MPI_Send((void*)(arr.data() + firstClusterLoad), remained, MPI_INT, 2, 4, MPI_COMM_WORLD); 
    cout << "M(0,2)\n";

    { // Calculate load for each worker and send to them.
        for (int i = 0; i < nrKids; ++i) {
            int toSend = i * load;
            int space = load;

            MPI_Send(&space, 1, MPI_INT, innerCluster[i], 0 + 3, MPI_COMM_WORLD);
            cout << "M(" << 0 << "," << innerCluster[i] << ")\n";
            MPI_Send((void*) (arr.data() + toSend), space, MPI_INT, innerCluster[i], 0 + 3, MPI_COMM_WORLD);
            cout << "M(" << 0 << "," << innerCluster[i] << ")\n";
        }

        for (int i = 0; i < nrKids; ++i) {
            int toSend = i * load;
            int space = load;

            MPI_Recv((void*) (arr.data() + toSend), space, MPI_INT, innerCluster[i], 0 + 3, MPI_COMM_WORLD, &status);
        }
    }

    MPI_Recv((void*)(arr.data() + firstClusterLoad), remained, MPI_INT, 2, 4, MPI_COMM_WORLD, &status); 
}

// Same as above, but for clsuters 1 2.
// Cluster 2 made 1 more receive.
// The order is 0 -> 2 -> 1, because of the alck of connection between
// cluster 0 and cluster 1.
void getAndProcess(map<int, int> mp,
                    int rank,
                    int from,
                    int arrSize,
                    vector<int> innerCluster) {
    int load, remained, sendRemained;
    MPI_Status status;
    vector<int> arr;
    int clusterLoad;
    int nrKids = numberOfChildren(rank, mp);

    MPI_Recv(&load, 1, MPI_INT, from, 4, MPI_COMM_WORLD, &status);
    MPI_Recv(&remained, 1, MPI_INT, from, 4, MPI_COMM_WORLD, &status);
    arr = vector<int>(remained);
    MPI_Recv((void*)arr.data(), remained, MPI_INT, from, 4, MPI_COMM_WORLD, &status);

    clusterLoad = min(load * nrKids, remained);
    sendRemained = remained - clusterLoad;

    if (rank == 2) {
        MPI_Send(&load, 1, MPI_INT, 1, 4, MPI_COMM_WORLD);
        cout << "M(2,1)\n";
        MPI_Send(&sendRemained, 1, MPI_INT, 1, 4, MPI_COMM_WORLD);
        cout << "M(2,1)\n";
        MPI_Send((void*)(arr.data() + clusterLoad), sendRemained, MPI_INT, 1, 4, MPI_COMM_WORLD); 
        cout << "M(2,1)\n";
    }
    
    { // Calcualte laod for each worker and send to them.
        // Used rank + 3 as tag, because of the big number of sends or recvs
        // that could happen and which can block the process.
        for (int i = 0; i < nrKids; ++i) {
            int toSend = i * load;
            int space = load;

            if (toSend >= clusterLoad) {
                break;
            }

            if (rank == 1 && i == nrKids - 1) {
                space = remained - toSend;
            }

            MPI_Send(&space, 1, MPI_INT, innerCluster[i], rank + 3, MPI_COMM_WORLD);
            cout << "M(" << rank << "," << innerCluster[i] << ")\n";
            MPI_Send((void*) (arr.data() + toSend), space, MPI_INT, innerCluster[i], rank + 3, MPI_COMM_WORLD);
            cout << "M(" << rank << "," << innerCluster[i] << ")\n";
        }

        for (int i = 0; i < nrKids; ++i) {
            int toSend = i * load;
            int space = load;

            if (toSend >= clusterLoad) {
                break;
            }

            if (rank == 1 && i == nrKids - 1) {
                space = remained - toSend;
            }

            MPI_Recv((void*) (arr.data() + toSend), space, MPI_INT, innerCluster[i], rank + 3, MPI_COMM_WORLD, &status);
        }
    }

    if (rank == 2) { // One more receive from cluster 1.
        MPI_Recv((void*)(arr.data() + clusterLoad), sendRemained, MPI_INT, 1, 4, MPI_COMM_WORLD, &status);
    }
    
    MPI_Send((void*)arr.data(), remained, MPI_INT, from, 4, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << from << ")\n";
}

// Process array for each worker.
void getAndMultiply(int ancestor, int rank) {
    int sz;
    vector<int> arr;
    MPI_Status status;

    MPI_Recv(&sz, 1, MPI_INT, ancestor, ancestor + 3, MPI_COMM_WORLD, &status);
    arr = vector<int>(sz);
    MPI_Recv((void*) arr.data(), sz, MPI_INT, ancestor, ancestor + 3, MPI_COMM_WORLD, &status);
    
    for (int i = 0; i < sz; ++i) {
        arr[i] = arr[i] * 2;
    }

    MPI_Send((void*) arr.data(), sz, MPI_INT, ancestor, ancestor + 3, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << ancestor << ")\n";
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Invalid usage\n";
        exit(1); 
    }

    int procs, rank;
    vector<int> innerCluster;
    map<int, int> parent;
    string filename;
    bool isConnected = atoi(argv[2]) == 1 ? true : false;
    int ancestor = -1;
    int arraySize = atoi(argv[1]);
    vector<int> arr(arraySize);

    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &procs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    filename = "cluster" + to_string(rank) + ".txt";

    if (rank < 3) {
        readChildren(filename, rank, innerCluster, parent);
    }

    parent[0] = parent[1] = parent[2] = -1;

    // Build topology.
    if (rank == 2) {
        secondClusterCoordConn(parent, innerCluster);
    } else if (rank < 3) {
        clusterConn(parent, innerCluster, rank);
    } else {
        getSystemForChild(rank, parent, ancestor);
    }

    // Build answer.
    if (rank == 0) {
        for (int i = 0; i < arraySize; ++i) {
            arr[i] = i;
        }

        processLoad(parent, arr, arraySize, innerCluster);

        cout << '\n';
    } else if (rank == 1) {
        getAndProcess(parent, rank, 2, arraySize, innerCluster);
    } else if (rank == 2) {
        getAndProcess(parent, rank, 0, arraySize, innerCluster);
    } else {
        getAndMultiply(ancestor, rank);
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (rank == 0) {
        cout << "Rezultat: ";

        for (auto it : arr) {
            cout << it << " ";
        }
    }

    MPI_Finalize();
}