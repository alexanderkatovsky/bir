# The 0th index is isfree
#           (0.0.0/1)         (1.0.0/2)         (0.1.0/1)
# 1  0 0 0    1 bit    0 1 0   2 bits    0 1 0    1 bit    0 1 1
# 2  0 0 0     -->     1 0 0     -->     1 0 1     -->     1 0 1
# 3  0 0 0             0 0 1             0 0 1             0 0 1 
#    
#    2 1 0


import math #,gmpy

class IP_Ex(Exception):
    def __init__(self,nbits):
        Exception.__init__(self,"No IP Address Space for Allocating %d bits"%nbits)

def SubnetToString(ip):
    return repr(int((ip >> 24) & 0xff)) + "." +\
        repr(int((ip >> 16) & 0xff)) + "." +\
        repr(int((ip >> 8) & 0xff)) + "." +\
        repr(int((ip) & 0xff))

class IP:
    def __init__(self,ip,nbits):
        self.__ip = ip
        self.__nbits = nbits
    
    def __repr__(self):
        SubnetToString(self.__ip) + "/" + repr(self.__nbits)
        
    def Subnet(self):
        return self.__ip
    def NBits(self):
        return self.__nbits
        

class IPMaker:
    def __init__(self, max_bits):
        self.__max = max_bits
        self.__current_value = {}
        for i in range(1,self.__max + 1):
            self.__current_value[i] = 0

    # def Print(self):
    #     print "max bits    :   " + repr(self.__max)
    #     print "current     :   "
    #     for i in range(1,self.__max + 1):
    #         print gmpy.digits(self.__current_value[i],2).zfill(self.__max)

    def isfree(self, nbits):
        return (self.__current_value[nbits] & 1 == 0)

    def setNotFree(self, nbits):
        self.__current_value[nbits] |= 1

    def iszero(self, nbits):
        return (self.__current_value[nbits] == 0)

    def setvalue(self, nbits, val):
        self.__current_value[nbits] = val

    def getvalue(self,nbits):
        return self.__current_value[nbits]

    def GetNW(self,nbits):
        if nbits > self.__max:
            raise IP_Ex(nbits)
        if nbits == self.__max:
            if self.isfree(nbits):
                self.setNotFree(nbits)
                return 0
            else:
                raise IP_Ex(nbits)
        if self.iszero(nbits) or not self.isfree(nbits):
            ip = self.GetNW(nbits+1)
            self.setvalue(nbits, ip | (1 << nbits))
            return ip
        ip = self.getvalue(nbits)
        self.setNotFree(nbits)
        return ip
        
    def GetNetwork(self,nnodes):
        nbits = int(math.ceil(math.log(nnodes + 1,2)))
        return IP(self.GetNW(nbits), nbits)
        

class IPMakerWithBase:
    def __init__(self, base, max_bits):
        self.__ipmaker = IPMaker(max_bits)
        self.__base = base
    def NextIP(self, nintfs):
        ip = self.__ipmaker.GetNetwork(nintfs)
        subnet = ip.Subnet() | self.__base
        return IP(subnet,ip.NBits())
