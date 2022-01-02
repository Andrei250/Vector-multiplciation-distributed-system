#include <iostream>
#include <fstream>
#include "mpi.h"
#include <unordered_map>
#include <vector>
#include <string>

using namespace std;

void readChildren(string filename, int rank,
                    vector<int>& innerCluster,
                    unordered_map<int, int>& parent) {
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

void sendToChildren(vector<int> innerConnection, int rank, vector<pair<int,int>> data) {
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

    cout << ans << '\n';
}

void secondClusterCoordConn(unordered_map<int, int>& parent, vector<int>& innerCone) {
    int numberOfElements;
    MPI_Status status;
    vector<int> elements;

    // Information from the first Cluster
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 0, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 0;
        }
    }
    
    // Information from the thirs Cluster
    {
        MPI_Recv(&numberOfElements, 1, MPI_INT, 1, 0, MPI_COMM_WORLD, &status);
        elements = vector<int>(numberOfElements);
        MPI_Recv((void*)elements.data(), numberOfElements * sizeof(int), MPI_BYTE, 1, 0, MPI_COMM_WORLD, &status);

        for (int it : elements) {
            parent[it] = 1;
        }
    }

    vector<pair<int, int>> mapPairs;
    int sz;

    for (auto it : parent) {
        mapPairs.push_back({it.first, it.second});
    }

    printSystem(2, mapPairs);
    sz = mapPairs.size();

    MPI_Send(&sz, 1, MPI_INT, 0, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 0 << ")\n";
    MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 0, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 0 << ")\n";

    MPI_Send(&sz, 1, MPI_INT, 1, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 1 << ")\n";
    MPI_Send((void*)mapPairs.data(), sz * sizeof(pair<int, int>), MPI_BYTE, 1, 0, MPI_COMM_WORLD);
    cout << "M(" << 2 << "," << 1 << ")\n";

    sendToChildren(innerCone, 2, mapPairs);
}

void clusterConn(unordered_map<int, int>& parent, vector<int> innerCluster, int rank) {
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

void getSystemForChild(int rank, unordered_map<int, int>& parent, int& ancestor) {
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

int numberOfChildren(int rank, unordered_map<int, int> mp) {
    int ans = 0;

    for (auto it : mp) {
        if (it.second == rank) {
            ans++;
        }
    }

    return ans;
}

void processLoad(unordered_map<int, int> parent, vector<int>& arr, int arrSize, vector<int> innerCluster) {
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

    {
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

void getAndProcess(unordered_map<int, int> mp, int rank, int from,  int arrSize, vector<int> innerCluster) {
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
    
    {
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

    if (rank == 2) {
        MPI_Recv((void*)(arr.data() + clusterLoad), sendRemained, MPI_INT, 1, 4, MPI_COMM_WORLD, &status);
    }
    
    MPI_Send((void*)arr.data(), remained, MPI_INT, from, 4, MPI_COMM_WORLD);
    cout << "M(" << rank << "," << from << ")\n";
}

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
    unordered_map<int, int> parent;
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

    if (rank == 2) {
        secondClusterCoordConn(parent, innerCluster);
    } else if (rank < 3) {
        clusterConn(parent, innerCluster, rank);
    } else {
        getSystemForChild(rank, parent, ancestor);
    }

    if (rank == 0) {
        for (int i = 0; i < arraySize; ++i) {
            arr[i] = i;
        }

        processLoad(parent, arr, arraySize, innerCluster);

        for (auto it : arr) {
            cout << it << " ";
        }

        cout << '\n';
    } else if (rank == 1) {
        getAndProcess(parent, rank, 2, arraySize, innerCluster);
    } else if (rank == 2) {
        getAndProcess(parent, rank, 0, arraySize, innerCluster);
    } else {
        getAndMultiply(ancestor, rank);
    }

    MPI_Finalize();
}