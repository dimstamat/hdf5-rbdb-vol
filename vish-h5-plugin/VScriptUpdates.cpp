/*
 * VScriptUpdates.cpp
 *
 * Contains functions for updating the state of the Vish network.
 * Was taken from the Vish ASCII loader and adopted to work with the HDF5 loader.
 *  Created on: Oct 31, 2016
 *      Author: dimos
 */


// for creating VObjects
#include <ocean/vscript/VScriptActions.hpp>
#include <ocean/vscript/VScriptInterface.hpp>
#include <ocean/vscript/VObjectSystemExecutor.hpp>
#include <ocean/vscript/VCreateScript.hpp>
#include <ocean/plankton/VObjectUserInfo.hpp>
#include <ocean/plankton/VLoader.hpp>
#include <ocean/plankton/VAcception.hpp>
#include <ocean/plankton/VScheduler.hpp>
#include <ocean/plankton/VModules.hpp>

using namespace std;
#include <iostream>

#include <memcore/Verbose.hpp>
#include "VScriptH5.hpp"
#include "VCreateScriptH5.hpp"

namespace Wizt{

	/* from ASCII VScript parser
	 * setup: IDENTIFIER '(' ')' */
	void setupObject(string& objName){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** setup: IDENTIFIER '(' ')'\n");
		#endif
		string	identifier = objName;
		#if H5_LOAD_DEBUG
		printf("SETUP CALL for [%s]\n", identifier.c_str() );
		#endif
		if (RefPtr<VObject> VObjSrc = VObject::find( identifier ) )
		{
			VObjSrc->initialize();
		}
	}
	/* from ASCII VScript parser
	 * attachment: member RIGHTARROW member */
	void addConnection(H5Script::MemberID& fromMember, H5Script::MemberID& toMember){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** attachment: member RIGHTARROW member\n");
		#endif
		RefPtr<VObject> VObjSrc = VObject::find( toMember.obj );
		if (!VObjSrc)
		{
			string	ObjName = VScriptEscapeIdentifierH5( toMember.obj);
			VObjSrc = VObject::find( ObjName );
			Verbose(0) << "Checking for " << ObjName;
		}
		if (VObjSrc)
		{
			if (RefPtr<VObject> VObjDst = VObject::find( fromMember.obj ) )
			{
			VObject::AttachErrorCode EC =
				VObjDst->attach(fromMember.member, *VObjSrc, toMember.member);

				switch(EC)
				{
				case VObject::AttachmentDisallowed:
				case VObject::AttachmentOk: break;
				case VObject::AttachmentToOutputOk:
					puts("VSCRIPT: Attached parameter to output property.");
					break;

				case VObject::SourceParameterNotFound:
					printf("VSCRIPT ERROR: Source parameter `%s' not found in `%s'\n",
						toMember.member.c_str(), toMember.obj.c_str());
					break;

				case VObject::DestParameterNotFound:
					puts("VSCRIPT ERROR: Destination parameter not found.");
					break;

				case VObject::InvalidDestParameter:
					puts("VSCRIPT ERROR: Destination Parameter found but invalid.");
					break;

				case VObject::InvalidSourceParameter:
					puts("VSCRIPT ERROR: Source Parameter found but invalid.");
					break;

				case VObject::IncompatibleType:
					puts("VSCRIPT ERROR: Source and destination parameters have incompatible types");
					break;
				}
			}
			else
			{ printf("ERROR in parameter attachment, cannot find source object [%s]!\n", fromMember.obj.c_str() ); }
		}
		else
		{ printf("[attachment: member RIGHTARROW member] ERROR in parameter attachment, cannot find parent object [%s]!\n", toMember.obj.c_str() ); }
	}

	/* from ASCII VScript parser
	 * creation: OBJECTCREATOR ' ' IDENTIFIER */
	void addObjectCreator(string& objectCreator, string& objName){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** creation: OBJECTCREATOR ' ' IDENTIFIER\n");
		#endif
		RefPtr<VCreatorBase> VC = VCreatorBase::find(objectCreator);
		string	idname = objName;
		VC->Name();
		#if H5_LOAD_DEBUG
		printf("Vcreate an object `%s' from <%s>!\n", idname.c_str(), VC->Name().c_str() );
		#endif
		assert( VC );
		Intercube	IC;
		RefPtr<VObject> vo = VC->create( idname, IC );
		assert(vo);
		if (VScriptInterface::isOnStartup)
		{
			vo->addInterface( new VScriptInterface() );
		}
	}
	/* from ASCII VScript parser
	* remove:  DELETEOBJECT IDENTIFIER */
	void deleteObject(string& objname) {
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** remove: DELETEOBJECT IDENTIFIER\n");
		#endif

	#if	0
			if (!VObject::remove( objname ) )
			{
				printf("   No object [%s] found!\n", objname.c_str() );
			}
	#else
			if (WeakPtr<VObject> VObj = VObject::find( objname ) )
			{
				printf("Deleting object: %s\n", objname.c_str());
				VObj->remove();
				if (VObj)
				{
					printf("   Could not remove [%s]!\n", objname.c_str() );
				}
			}
	/*
			else
			{
				printf("   No object [%s] found!\n", objname.c_str() );
			}
	*/
	#endif
	}
	/* from ASCII VScript parser
	 * statement: RIGHTARROW member '=' IDENTIFIER */
	void addParamLocal(H5Script::MemberID& mID, RefPtr<VValueBase> value, H5Script& h5script){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints ****  statement: RIGHTARROW member '=' IDENTIFIER\n");
		#endif
		string	objname = mID.obj,
		parname = mID.member;

		Verbose(58) << "VScript Object Assign Member Locally: objname=" << objname
				<< " parname="  << parname
					<< " value="    << value.to_string();

		RefPtr<VObject> VObj = VObject::find( objname );
			if (VObj)
			{
			RefPtr<VParameter> Vparam = VObj->getParameter( parname );
				if (Vparam)
				{
					struct AssignmentAction : H5Script::Action
					{
						RefPtr<VParameter>	Vparam;
						RefPtr<VValueBase>	value;

						AssignmentAction(const RefPtr<VParameter>&Vpar,
								 const RefPtr<VValueBase>&val)
						: Vparam(Vpar), value(val)
						{}

						RefPtr<VScriptValue> exec()
						{
						RefPtr<VValueBase> VB = Vparam->getGlobalValue();
							if (!VB)
								return NullPtr();

							Vparam->Localize();
							bool set_ok = false;
							if (value->getType() != typeid(int) && value->getType() != typeid(double) && value->getType() != typeid(bool)){ // set value from text! That's either unknown type or string
								set_ok = VB->setValueFromText(value->Text());
							}
							else {
								if(VB->getType() != value->getType()){
									Verbose(0)<<"WARNING: Type mismatch, type already: " << typeid(VB->getType()) <<", type to be set: "<< typeid(value->getType());
								}
								set_ok = Vparam->setGlobalValue(value, "");
								//set_ok = (VB = value);
							}
							if (!set_ok )
							{
								Verbose(0) << "VSCRIPT: (addParamLocal::exec: Could not set global parameter value to " << value.to_string();
								return NullPtr();
							}
							Verbose(45)<< "Assigned value (addParamLocal): " << VB->Text() << "(" << VB->getType() << ")" << " <- " << value->Text() << " (" << value->getType() <<")";
							Vparam->notifyInputs(NullPtr(),0);
							Vparam->touchValue(NullPtr());

							RefPtr<VScriptValue> scriptvalue = new VScriptValue()  ;

							scriptvalue -> insert( Vparam->getType(), new VScriptTypedValue(value.to_string()) );

							return scriptvalue;
						}
					};
					h5script.action = new AssignmentAction(Vparam, value);
				}
				else
					h5script.action = 0;
			}
			else
				h5script.action = 0;
	}
	/* from ASCII VScript parser
	 * statement: member '=' IDENTIFIER */
	void addParamSimple(H5Script::MemberID& mID, RefPtr<VValueBase> value, H5Script& h5script){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints ****  member '=' IDENTIFIER\n");
		printf("obj: %s, value: %s\n", mID.obj.c_str(), value.to_string().c_str());
		#endif
		RefPtr<VObject> VObj = VObject::find( mID.obj );
		if (VObj)
		{
		RefPtr<VParameter> Vparam = VObj->getParameter( mID.member );
			if (Vparam)
			{
				struct AssignmentAction : H5Script::Action
				{
					RefPtr<VParameter>	Vparam;
					RefPtr<VValueBase>	value;

					AssignmentAction(const RefPtr<VParameter>&Vpar,
							 const RefPtr<VValueBase>&val)
					: Vparam(Vpar), value(val)
					{}

					RefPtr<VScriptValue> exec() {
						RefPtr<VValueBase> VB = Vparam->getGlobalValue();
						if (!VB)
							return NullPtr();
						Vparam->Globalize(nullptr);
						bool set_ok = false;
						if (value->getType() != typeid(int) && value->getType() != typeid(double) && value->getType() != typeid(bool)){ // set value from text! That's either unknown type or string
							set_ok = VB->setValueFromText(value->Text());
						}
						else {
							set_ok = Vparam->setGlobalValue(value, "");
							//set_ok = (VB = value); // this only updates the local variable VB!
							if(VB->getType() != value->getType()){
								Verbose(0)<<"WARNING: Type mismatch, type already: " << typeid(VB->getType()) <<", type to be set: "<< typeid(value->getType());
							}
						}
						if (!set_ok)
						{
							Verbose(0) << "VSCRIPT: addParamSimple::exec: Could not set global parameter value to " << value->Text() << " (VB after new value assignment: " <<VB->Text() <<")";
							return NullPtr();
						}
						Verbose(45)<< "Assigned value (addParamSimple): " << VB->Text() << "(" << VB->getType() << ")" << " <- " << value->Text() << " (" << value->getType() <<")";
						Verbose(45)<< "Now val: "<< VB->Text();
						Vparam->notifyInputs(NullPtr(),0);
						Vparam->touchValue(NullPtr());
						RefPtr<VScriptValue> scriptvalue = new VScriptValue()  ;
						scriptvalue -> insert( Vparam->getType(), new VScriptTypedValue(value.to_string()) );

						return scriptvalue;
					}
				};
				h5script.action = new AssignmentAction(Vparam, value);
			}
			else
				h5script.action = 0;
		}
		else
			h5script.action = 0;
	}
	/* assignment relative to some context
	 * from ASCII VScript parser
	 * context: '{' IDENTIFIER '}'
	 * statement: member context '=' IDENTIFIER */
	void addParamContext(string& objName, string& paramName, string& contextName, RefPtr<VValueBase> value, H5Script& h5script){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** context: '{' IDENTIFIER '}'\n");
		printf("Adding %s (context: %s)=%s to object %s\n", paramName.c_str(), contextName.c_str(), value.to_string().c_str(), objName.c_str());
		#endif
		string	varpool = contextName;
		h5script.valuepool = new RefPtr<ValuePool>( ValuePool::getValuePool(varpool) );
		if (!*h5script.valuepool)
		{
			printf("VSCRIPT WARNING: No such value pool %s\n", varpool.c_str());
		}

		RefPtr<ValuePool> VP = *h5script.valuepool;
		if (!VP)
		{
			printf("VSCRIPT: No value context found - cannot assign context-relative value!\n");
			fflush(stdout);
			h5script.action = 0;
		}
		else
		{
			//	puts("VSCRIPT STATEMENT: Assign member in context");
			RefPtr<VObject> VObj = VObject::find( objName );
			if (VObj)
			{
				RefPtr<VParameter> Vparam = VObj->getParameter( paramName );
				if (Vparam)
				{
					printf("-> Parameter found! Adding action!\n");
					struct AssignmentAction : H5Script::Action
					{
						RefPtr<VParameter>	Vparam;
						RefPtr<VValueBase>  value;
						RefPtr<ValuePool>	VP;

						AssignmentAction(const RefPtr<VParameter>&Vpar,
								 const RefPtr<VValueBase> &val,
								 const RefPtr<ValuePool>&vp)
						: Vparam(Vpar), value(val), VP(vp)
						{}

						RefPtr<VScriptValue> exec()
						{
							AppVerbose("Dimos", 10) << " **** Executing action " << value;
							if (value->getType() != typeid(int) && value->getType() != typeid(double) && value->getType() != typeid(bool) ){
								if (!Vparam->setValueFromText( value->Text(), VP, "", true, NullPtr() ) )
								return NullPtr();
							}
							else if( ! Vparam->setValue(value, VP, "", true, nullptr))
								return NullPtr();
						// already done				Vparam->notifyInputs(0, VP);
						//							Vparam->touch();
						RefPtr<VScriptValue> scriptvalue = new VScriptValue()  ;
							scriptvalue -> insert( Vparam->getType(), new VScriptTypedValue(value.to_string()) );

							return scriptvalue;
						}
					};
					h5script.action = new AssignmentAction(Vparam, value, VP);
				}
				else
					h5script.action = 0;
			}
			else
				h5script.action = 0;
		}
	}
	/* from ASCII VScript parser
	 * parameterproperty: member '.' IDENTIFIER '(' IDENTIFIER ')' '=' IDENTIFIER */
	void addParamProperty(H5Script::MemberID& mID, string& parname, string& propertyType, string& propname, RefPtr<VValueBase> value){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints **** parameterproperty: member '.' IDENTIFIER '(' IDENTIFIER ')' '=' IDENTIFIER\n");
		#endif
		string	objname  = mID.obj;

		#if H5_LOAD_DEBUG
		printf("Reading property [%s] for object [%s]->[%s], type is [%s], value is [%s]\n" , propname.c_str(), objname.c_str(), parname.c_str(), propertyType.c_str(), value.to_string().c_str());
		#endif

		if (RefPtr<VObject> VObj = VObject::find( objname ))
		{
			if (RefPtr<VSlot> MyInputSlot = VObj->getSlot( parname ) )
			{
				if (RefPtr<VValueBase> myProperty =
					MyInputSlot -> getProperty( propname ))
				{
					bool set_ok = false;
					if (RefPtr<VValue<int>> myPropertyInt = value)
						set_ok = MyInputSlot->setProperty(propname, (int) (*myPropertyInt));
					else if (RefPtr<VValue<double>> myPropertyDouble = value)
						set_ok = MyInputSlot->setProperty(propname, (double) (*myPropertyDouble));
					else if (RefPtr<VValue<bool>> myPropertyBool = value)
						set_ok = MyInputSlot->setProperty(propname, (bool) (*myPropertyBool));
					else { // set value from text
						set_ok = myProperty->setValueFromText(value->Text());
						if(myProperty->getType() != value->getType()){
							Verbose(0)<<"WARNING: Type mismatch, type already: " << typeid(myProperty->getType()) <<", type to be set: "<< typeid(value->getType());
						}
					}
					if (set_ok)
					{
						if (RefPtr<VParameter> Par = MyInputSlot->getParameter() )
						{
							Par->sendNotification();
						}
						Verbose(45)<< "Assigned value (addParamProperty): " << myProperty->Text() << "(" << myProperty->getType() << ")" << " <- " << value->Text() << " (" << value->getType() <<")";
					}
					else
					{
						Verbose(0) << "Assign parameter property: Could not set  property '"+propname+
						"' of slot '"+parname+"' in object '"+objname+"' to value '"+value.to_string()+"'!";
					}
				}
				else
				{
					if (RefPtr<VParameter> Par = MyInputSlot->getParameter() )
					{
						Par->setProperty(propname, value);
						Verbose(50) << "Assign parameter property: OK - creating value from '"
						   << value << "' as type '" << propertyType << "': "
						   << value;

						#ifdef	VERBOSE_VSCRIPT_PARSER
						printf("Assign parameter property: YESS - created value from '%s' as type '%s'.\n",
									value.to_string(), propertyType.c_str());
						#endif

					}
					else
					{
						printf("Assign parameter property: No parameter available for slot.\n");
					}
					/*
					bool	conversion_successful = false;
					if (WeakPtr<VValueBase,VValueBase> PropertyValue =
						VValueCreator::create(propertyType, value, conversion_successful) )
					{
						if (conversion_successful)
						{
							if (RefPtr<VParameter> Par = MyInputSlot->getParameter() )
							{
								Par->setProperty(propname, PropertyValue);
								Verbose(50) << "Assign parameter property: OK - creating value from '"
								   << value << "' as type '" << propertyType << "': "
								   << PropertyValue;

	#ifdef	VERBOSE_VSCRIPT_PARSER
								printf("Assign parameter property: YESS - created value from '%s' as type '%s'.\n",
											value.c_str(), propertyType.c_str());
	#endif

							}
							else
							{
								printf("Assign parameter property: No parameter available for slot.\n");
							}
						}
						else
						{
							Verbose(0) << "Assign parameter property: Problem in creating value from '"
								   << value << "' as type '" << propertyType << "'.";
							printf("Assign parameter property: Problem in creating value from '%s' as type '%s'.\n",
							value.c_str(), propertyType.c_str());
						}
					}
					else
					{
						Verbose(0) << "Assign parameter property: Could not create value from '"+value+"' as type '"+propertyType+"'.";
	//						printf("Assign parameter property: Slot '%s' in object '%s' has no property '%s'.\n",
	//									parname.c_str(), objname.c_str(), propname.c_str() );
					}*/
				}
			}
			else
			{
				Verbose(0) << "Assign parameter property: No slot named '" << parname << "' in object '"<<objname<<"' found.";
				printf("Assign parameter property: Slot '%s' in object '%s' NOT found\n",
					parname.c_str(), objname.c_str());
			}
		}
		else
		{
			Verbose(0) << "Assign parameter property: No object ["<<objname<<"] found.";
			printf("Assign parameter property: No object '%s' found\n", objname.c_str());
		}
	}
	/* from ASCII VScript parser
	 * scriptvalue: statement */
	void addStatementToScriptValue(H5Script& h5script){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints ****  scriptvalue: statement\n");
		#endif
		if (h5script.action) {
			#if H5_LOAD_DEBUG
			printf("\t\t VSCRIPT STATEMENT: Evaluating Valid Statement\n");
			#endif
			RefPtr<VScriptValue> *scriptvalue = new RefPtr<VScriptValue>( h5script.action->exec() );
			//delete h5script.action;
			h5script.scriptvalue = scriptvalue;
		}
		else
			h5script.scriptvalue = 0;
	}
	/* from ASCII VScript parser
	 * discardvalue: scriptvalue */
	void addScriptValueToDiscardvalue(H5Script& h5script){
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints ****  discardvalue: scriptvalue\n");
		#endif
		if (h5script.scriptvalue) {
			if (*h5script.scriptvalue)
			{
				struct Vit : VScriptValue::iterator
				{
					bool apply(const type_info&what, const VScriptTypedValue&V) override
					{
						#if H5_LOAD_DEBUG
						printf(" **** Dimos prints **** discardvalue: scriptvalue: Evaluation result \"%s\":  %s\n", Typename(what).c_str(), V.value.c_str() );
						#endif
						return true;
					}
				};
				Vit Vittt;
				(*h5script.scriptvalue)->iterate(Vittt);
			}
			else
			{
				#if H5_LOAD_DEBUG
				puts("VSCRIPT: Function call returned (void)");
				#endif
			}
		}
		// discard script value
		delete h5script.scriptvalue;
	}

	/**
	 * adds the given parameter and value in the current objName
	 * objName: the name of the current object
	 * param: the parameter we want to add
	 * value: the value of the parameter
	 * isLocal: 1 if parameter is local, 0 otherwise
	 */
	void addParam(string& objName, string& param, RefPtr<VValueBase> value, bool isLocal, H5Script& h5script) {
		size_t left_par_pos, right_par_pos, left_brace_pos, right_brace_pos;
		size_t dot_pos = param.find(".");
		string paramName = param.substr(0, dot_pos);
		/* from ASCII Vscript parser
		 * member: IDENTIFIER '.' IDENTIFIER */
		H5Script::MemberID mID = H5Script::MemberID( objName, paramName);
		#if H5_LOAD_DEBUG
		printf(" **** Dimos prints ****  IDENTIFIER '.' IDENTIFIER\n");
		printf("Member: [%s]->[%s]\n", mID.obj.c_str(), mID.member.c_str() );
		#endif
		if(isLocal){
			addParamLocal(mID, value, h5script);
		}
		else if(  (left_par_pos = param.find("(")) != string::npos && (right_par_pos = param.find(")")) != string::npos){ // there are parentheses, case of IDENTIFIER ( IDENTIFIER ) = IDENTIFIER
			string propertyType, propertyName;
			size_t  dot_pos = param.find(".");
			assert(dot_pos != string::npos);
			propertyName = param.substr(dot_pos+1, left_par_pos-dot_pos-1);
			propertyType = param.substr(left_par_pos+1, right_par_pos - left_par_pos-1);
			addParamProperty(mID, paramName, propertyType, propertyName, value);
		}
		else if ( (left_brace_pos = param.find("{")) != string::npos && (right_brace_pos = param.find("}")) != string::npos){ // there are braces, case of context: { IDENTIFIER }
			string contextName;
			contextName = param.substr(left_brace_pos+1, right_brace_pos - left_brace_pos-1);
			paramName = paramName.substr(0, left_brace_pos);
			addParamContext(objName, paramName, contextName, value, h5script);
		}
		else {
			addParamSimple(mID, value, h5script);
		}
		addStatementToScriptValue(h5script);
		addScriptValueToDiscardvalue(h5script);
	}
}
