import uuid
import traceback

import protocol.Sirikata_pb2 as pbSiri
import protocol.Persistence_pb2 as pbPer
import protocol.MessageHeader_pb2 as pbHead

import util

class exampleclass:
    def __init__(self):
        self.objid=0

    def sawAnotherObject(self,persistence,header,retstatus):
        if header.HasField('return_status') or retstatus:
            return
        uuid = uuid.UUID(bytes=header.source_object)
        myName = ""
        for field in persistence:
            if field.field_name == 'Name':
                if field.HasField('data'):
                    myName = field.data
        print "PY: Object",uuid,"has name",myName

    def locreqCallback(self, headerser, bodyser):
        hdr = pbHead.Header()
        hdr.ParseFromString(util.fromByteArray(headerser))
        body = pbSiri.MessageBody()
        body.ParseFromString(util.fromByteArray(bodyser))
        print "PY: locreqCallback, type, lengths:", type(headerser), len(headerser), type(bodyser), len(bodyser)
        print "PY: locreqCallback hdr:", type(hdr), hdr.return_status
        print "PY: locreqCallback len(body.message_arguments) =", len(body.message_arguments)
        print "PY: locreqCallback type(body.message_arguments) =", type(body.message_arguments)
        print "PY: locreqCallback type(body.message_arguments[0]) = ", type(body.message_arguments[0])
        print "PY: locreqCallback body.message_arguments[0] =", body.message_arguments[0]
##        print "PY: locreqCallback body.ListFields:", body.ListFields()
##        print "PY: locreqCallback  field descriptor:", type(body.ListFields()[0][0]), "|||", body.ListFields()[0][0].full_name
##        print "PY: locreqCallback   field container:", type(body.ListFields()[0][1]), "|||", type(body.ListFields()[0][1][0])
        return False

    def reallyProcessRPC(self,serialheader,name,serialarg):
        print "Got an RPC named",name
        header = pbHead.Header()
        header.ParseFromString(util.fromByteArray(serialheader))
        if name == "RetObj":
            retobj = pbSiri.RetObj()
            print repr(util.fromByteArray(serialarg))
            try:
                retobj.ParseFromString(util.fromByteArray(serialarg))
            except:
                pass
            self.objid = retobj.object_reference
            print "sendprox1"
            self.spaceid = header.source_space
            print "PY: space UUID:", uuid.UUID(bytes=self.spaceid)
            self.setPosition(angular_speed=1,axis=(0,1,0))

        elif name == "ProxCall":
            proxcall = pbSiri.ProxCall()
            proxcall.ParseFromString(util.fromByteArray(serialarg))
            objRef = uuid.UUID(bytes=proxcall.proximate_object)
            print "PY: Proxcall on:", objRef

            #not functional yet
            """
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
            """
            
            if self.objid and (proxcall.proximate_object!=self.objid):
                print "PY: sending LocRequest"
                body = pbSiri.MessageBody()
                body.message_names.append("LocRequest")
                args = pbSiri.LocRequest()
                args.requested_fields=3
                body.message_arguments.append(args.SerializeToString())
                header = pbHead.Header()
                header.destination_space = self.spaceid
                header.destination_object = proxcall.proximate_object #self.objid
                HostedObject.CallFunction(util.toByteArray(header.SerializeToString()+body.SerializeToString()), self.locreqCallback)

            print uuid.UUID(bytes=self.spaceid)
            self.sendNewProx()
            self.setPosition(angular_speed=1,axis=(0,1,0))
        elif name == "ProxCall":
            proxcall = pbSiri.ProxCall()
            proxcall.ParseFromString(util.fromByteArray(serialarg))
            objRef = uuid.UUID(bytes=proxcall.proximate_object)
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
    def sawAnotherObject(self,persistence,header,retstatus):
        if header.HasField('return_status') or retstatus:
            return
        uuid = uuid.UUID(bytes=header.source_object)
        myName = ""
        for field in persistence:
            if field.field_name == 'Name':
                if field.HasField('data'):
                    myName = field.data
        print "Object",uuid,"has name",myName
    def processRPC(self,header,name,arg):
        try:
            self.reallyProcessRPC(header,name,arg)
        except:
            print "Error processing RPC",name
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
