import uuid
import traceback

import protocol.Sirikata_pb2 as pbSiri
import protocol.Persistence_pb2 as pbPer
import protocol.MessageHeader_pb2 as pbHead

from Sirikata.Runtime import HostedObject
import array

import util

class Struct:
    pass

class SirikataObjectScript:

    def __init__(self):
        self.known_objects={}               # will fill with location info as we discover 'em

    def locreqCallback(self, headerser, bodyser):
        hdr = pbHead.Header()
        hdr.ParseFromString(util.fromByteArray(headerser))
##        print "PY header:", type(hdr), dir(hdr)
        objID = util.tupleToUUID(hdr.source_object)
        print "PY locreqCb source obj:", objID
##        self.known_objects[objID].position = fuk
        body = pbSiri.MessageBody()
        body.ParseFromString(util.fromByteArray(bodyser))
        response = pbSiri.ObjLoc()
        response.ParseFromString(util.fromByteArray(body.message_arguments[0]))
        obj = self.known_objects[objID]
        if len(response.position):
            obj.position=response.position
            print "PY: locreqCallback, position:", response.position[0], response.position[1], response.position[2]
        if len(response.orientation):
            obj.orientation=decode_quaternion(response.orientation)
        if len(response.velocity):
            obj.velocity=response.velocity
            print "PY: locreqCallback, velocity:", obj.velocity[0], obj.velocity[1], obj.velocity[2]
        if len(response.rotational_axis):
            obj.rotational_axis=decode_rotational_axis(response.rotational_axis)
            obj.angular_speed=response.angular_speed        #lazy
        return False

    def sendLocRequest(self, obj, mask):
        ### mask: 1 = position, 2=orientation, 4=velocity, 8=axis, 16=ang vel
        body = pbSiri.MessageBody()
        body.message_names.append("LocRequest")
        args = pbSiri.LocRequest()
        args.requested_fields=mask
        body.message_arguments.append(args.SerializeToString())
        header = pbHead.Header()
        header.destination_space = self.spaceid             #one space for now
        header.destination_object = obj #self.objid
        HostedObject.CallFunction(util.toByteArray(header.SerializeToString()+body.SerializeToString()), self.locreqCallback)

    def reallyProcessRPC(self,serialheader,name,serialarg):
        print "PY: Got an RPC named-->" + name + "<--"
        header = pbHead.Header()
        header.ParseFromString(util.fromByteArray(serialheader))

        if name == "RetObj":
            retobj = pbSiri.RetObj()
            #print repr(util.fromByteArray(serialarg))
            try:
                retobj.ParseFromString(util.fromByteArray(serialarg))
            except:
                pass
            self.objid = retobj.object_reference
            print "PY: I am", util.tupleToUUID(self.objid)
            self.spaceid = header.source_space
            print "PY: space UUID:", util.tupleToUUID(self.spaceid)
            self.sendNewProx()
##            self.setPosition(angular_speed=1,axis=(0,1,0))

        elif name == "ProxCall":
            proxcall = pbSiri.ProxCall()
            proxcall.ParseFromString(util.fromByteArray(serialarg))
            objRef = util.tupleToUUID(proxcall.proximate_object)
            if not objRef in self.known_objects:
                self.known_objects[objRef] = Struct()
            self.known_objects[objRef].objTup=proxcall.proximate_object
            print "PY: Proxcall on:", objRef
            if proxcall.proximity_event == pbSiri.ProxCall.ENTERED_PROXIMITY:
                myhdr = pbHead.Header()
                myhdr.destination_space = self.spaceid
                myhdr.destination_object = self.objid
                dbQuery = util.PersistenceRead(self.sawAnotherObject)
                field = dbQuery.reads.add()
                field.field_name = 'Name'
                dbQuery.send(HostedObject, myhdr)
            if proxcall.proximity_event == pbSiri.ProxCall.EXITED_PROXIMITY:
                pass

            if self.objid and (proxcall.proximate_object!=self.objid):
                print "PY: sending LocRequest"
                self.sendLocRequest(proxcall.proximate_object, 1+4)

    def hexdump(self, data, fn):
        b = array.array("B",[])
        for i in data:
            b.append(i)
        f = open(fn,"wb")
        f.write(b.tostring())
        f.close()

    def sawAnotherObject(self,persistence,header,retstatus):
        if header.HasField('return_status') or retstatus:
            return
        uuid = util.tupleToUUID(header.source_object)
        myName = ""
        for field in persistence:
            if field.field_name == 'Name':
                if field.HasField('data'):
                    myName = field.data
        print "PY: Object",uuid,"has name",myName

    def processRPC(self,header,name,arg):
        try:
            self.reallyProcessRPC(header,name,arg)
        except:
            print "PY: Error processing RPC",name
            traceback.print_exc()

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
            header.destination_object = util.tupleFromUUID(uuid.UUID(int=0))
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
