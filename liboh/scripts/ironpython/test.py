import uuid
import traceback
import protocol.Sirikata_pb2 as pbSiri
import protocol.MessageHeader_pb2 as pbHead
from System import Array, Byte
from Sirikata.Runtime import HostedObject

def fromByteArray(b):
    return ''.join(chr(c) for c in b)

def toByteArray(p):
    return Array[Byte](tuple(Byte(ord(c)) for c in p))


class exampleclass:

    def reallyProcessRPC(self,serialheader,name,serialarg):
        print "PY:", "Got an RPC named-->" + name + "<--"
        header = pbHead.Header()
        header.ParseFromString(fromByteArray(serialheader))
        if name == "RetObj":
            retobj = pbSiri.RetObj()
            try:
                retobj.ParseFromString(fromByteArray(serialarg))
            except:
                pass
            self.objid = retobj.object_reference
            print "PY: my UUID is:", uuid.UUID(bytes=self.objid)
            self.spaceid = header.source_space
            print "PY: space UUID:", uuid.UUID(bytes=self.spaceid)
            print "PY: sending LocRequest"
            body = pbSiri.MessageBody()
            body.message_names.append("LocRequest")
            args = pbSiri.LocRequest()
            args.requested_fields=1
            body.message_arguments.append(args.SerializeToString())
            header = pbHead.Header()
            header.destination_space = self.spaceid
            header.destination_object = self.objid
            HostedObject.CallFunction(toByteArray(header.SerializeToString()+body.SerializeToString()), self.callback)

    def processRPC(self,header,name,arg):
        try:
            self.reallyProcessRPC(header,name,arg)
        except:
            print "PY:", "Error processing RPC",name
            traceback.print_exc()

    def callback(self, headerser, bodyser):
        print "PY callback, head:", len(headerser), "body:", len(bodyser)
        return false

    def setPosition(self,position=None,orientation=None,velocity=None,angular_speed=None,axis=None,force=False):
        objloc = pbSiri.ObjLoc()
        if position is not None:
            for i in range(3):
                objloc.position.append(position[i])
        if velocity is not None:
            for i in range(3):
                objloc.velocity.append(velocity[i])
        if orientation is not None:
            total = 0
            for i in range(4):
                total += orientation[i]*orientation[i]
            total = total**.5
            for i in range(3):
                objloc.orientation.append(orientation[i]/total)
        if angular_speed is not None:
            objloc.angular_speed = angular_speed
        if axis is not None:
            total = 0
            for i in range(3):
                total += axis[i]*axis[i]
            total = total**.5
            for i in range(2):
                objloc.rotational_axis.append(axis[i]/total)
        if force:
            objloc.update_flags = pbSiri.ObjLoc.FORCE
        body = pbSiri.MessageBody()
        body.message_names.append("SetLoc")
        body.message_arguments.append(objloc.SerializeToString())
        header = pbHead.Header()
        header.destination_space = self.spaceid
        header.destination_object = self.objid
        HostedObject.SendMessage(util.toByteArray(header.SerializeToString()+body.SerializeToString()))
    def sendNewProx(self):
        print "sendprox2"
        try:
            print "sendprox3"
            body = pbSiri.MessageBody()
            prox = pbSiri.NewProxQuery()
            prox.query_id = 123
            print "sendprox4"
            prox.max_radius = 1000.0
            body.message_names.append("NewProxQuery")
            body.message_arguments.append(prox.SerializeToString())
            header = pbHead.Header()
            print "sendprox5"
            header.destination_space = self.spaceid;
            header.destination_object = uuid.UUID(int=0).get_bytes()
            header.destination_port = 3 # libcore/src/util/KnownServices.hpp
            headerstr = header.SerializeToString()
            bodystr = body.SerializeToString()
            HostedObject.SendMessage(util.toByteArray(headerstr+bodystr))
        except:
            print "ERORR"
            traceback.print_exc()

    def processMessage(self,header,body):
        print "Got a message"
    def tick(self,tim):
        x=str(tim)
        print "Current time is "+x;
        #HostedObject.SendMessage(tuple(Byte(ord(c)) for c in x));# this seems to get into hosted object...but fails due to bad encoding
