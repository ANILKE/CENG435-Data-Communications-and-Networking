import sys

import socket

import time

import threading



local_host = "127.0.0.1"





class Port:

    def __init__(self, portNumber, totalPortNumber, neighbours, neighbourCosts, isChanged, last_modified_time,

                 routerTable, neigh_router_table, neigh_port):

        self.portNumber = portNumber

        self.totalPortNumber = totalPortNumber

        self.neighbours = neighbours

        self.neighbourCosts = neighbourCosts

        self.isChanged = isChanged

        self.last_modified_time = last_modified_time

        self.routerTable = routerTable

        self.neigh_router_table = neigh_router_table

        self.neigh_port = neigh_port



    def create_router_table(self):



        self.routerTable = [[100000] * self.totalPortNumber for i in range(self.totalPortNumber)]

        self.neigh_router_table = [[100000] * self.totalPortNumber for i in range(self.totalPortNumber)]

        self.routerTable[self.portNumber % 3000][self.portNumber % 3000] = 0

        for i in range(len(self.neighbours)):

            self.routerTable[self.portNumber % 3000][self.neighbours[i] % 3000] = self.neighbourCosts[i]



    def parse_input(self, f):

        i = 0

        global total_port_no

        for x in f:

            if (i == 0):

                self.totalPortNumber = int(x)

                i += 1

            else:

                x = x.split()

                self.neighbours.append(int(x[0]))

                self.neighbourCosts.append(int(x[1]))



    def distance_vector_routing(self):

        global total_port_no

        for i in range(self.totalPortNumber):

            self.last_modified_time = time.time()

            mini = 100000

            mini = min(mini, (self.routerTable[self.portNumber % 3000][self.neigh_port % 3000] +

                              self.neigh_router_table[self.neigh_port % 3000][i]))



            if self.routerTable[self.portNumber % 3000][i] > mini:

                self.routerTable[self.portNumber % 3000][i] = mini

                self.isChanged = True

        self.last_modified_time = time.time()



    def parse_neigh_router(self, data):

        data = data.split(',')

        for i in range(self.totalPortNumber):

            for j in range(self.totalPortNumber):

                self.neigh_router_table[i][j] = int(data[i * self.totalPortNumber + j])



    def print_distance_vectors(self):

        for i in range(self.totalPortNumber):

            if (self.routerTable[self.portNumber % 3000][i] != 100000):

                print("{} -{} | {}".format(self.portNumber, i + 3000, self.routerTable[self.portNumber % 3000][i]))

        print("\n")

    

    def set_neigh_port(self):

        for i in range(self.totalPortNumber):

            for j in range(self.totalPortNumber):

                if(self.neigh_router_table[i][j] == 0):

                    self.neigh_port = i 



# global router

curr_port = Port(0, 0, [], [], False, time.time(), [], [], 0)

is_finished = False







def thread_listen():

    global is_finished

    lock = threading.Lock()

    s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    #Warning

    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    # Warning

    s.bind((local_host, curr_port.portNumber))

    s.listen(curr_port.totalPortNumber)

    s.settimeout(5)

    try:

        while True:

            if (is_finished):

                

                break

            for i in range(len(curr_port.neighbours)):

                client_socket, addr = s.accept()

                curr_port.last_modified_time = time.time()

                

                data = client_socket.recv(1024).decode('utf-8')

                client_socket.close()

                if (data == ""):

                    break

                

                curr_port.parse_neigh_router(data)

                curr_port.set_neigh_port()

                

                curr_port.distance_vector_routing()

                curr_port.last_modified_time = time.time()

                

    except:

        return

        

    return





def thread_send():

    global is_finished

    while True:

        if (is_finished):

            break

        if (curr_port.isChanged):

            curr_port.isChanged = False

            data = ""

            for row in range(curr_port.totalPortNumber):

                for column in range(curr_port.totalPortNumber):

                    data = data + "{},".format(curr_port.routerTable[row][column])

            i = 0

            while i <len(curr_port.neighbours):

                try:

                    sending_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

                    sending_socket.connect((local_host, curr_port.neighbours[i]))



                    sending_socket.sendall(data.encode('utf-8'))

                    sending_socket.close()

                    i += 1

                except:

                    pass

            

    return





def timeout_timer():

    global is_finished

    while True:

        if (time.time() - curr_port.last_modified_time >= 5):

           

            is_finished = True

            

            time.sleep(0.1*(curr_port.portNumber-3000))

            curr_port.print_distance_vectors()



            break

    return





if __name__ == '__main__':

    curr_port.portNumber = int(sys.argv[1])

    f = open("./{}.costs".format(curr_port.portNumber), "r")

    curr_port.parse_input(f)

    f.close()

    curr_port.create_router_table()



    listening = threading.Thread(target=thread_listen, args=())

    sending = threading.Thread(target=thread_send, args=())

    timer = threading.Thread(target=timeout_timer, args=())



    curr_port.isChanged = True



    listening.start()

    time.sleep(1)

    curr_port.last_modified_time = time.time()

    sending.start()

    timer.start()









