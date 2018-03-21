#include "VActivityTracer.hpp"
#include <ocean/plankton/VCreator.hpp>
#include "VScriptH5Saver.hpp"

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
#include <ocean/vscript/VScriptParser.hpp>

#include <memcore/Verbose.hpp>

#include "VCreateScriptH5.hpp"

#include <hdf5.h>

namespace Wizt
{
	struct myNotifier : VActionNotifier 
	{
		WeakPtr<VActivityTracer> Me;
		hid_t  		script_id;
		hid_t		string_id;
		unsigned	ExecID;

	private:
		myNotifier();
		myNotifier(const myNotifier&);

	public:
		myNotifier(const WeakPtr<VActivityTracer>&T)
		: Me(T)
		, ExecID(0)
		{
		hid_t	FileID = H5Fcreate( "VishLog.v5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); 
		hid_t	VishGroup  = H5Gcreate2( FileID, "VishActions", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);

//			strftime ( entry, sizeof(entry), const char * format, const struct tm * timeptr );

			script_id  = H5Gcreate2( VishGroup, "today", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); 
			H5Gclose( VishGroup );
			printf("VActivity Tracer, closing HDF5 file!\n");
			H5Fclose( FileID ); 

			string_id = H5Tcopy(H5T_C_S1); 
			H5Tset_size( string_id, H5T_VARIABLE);
		}


		~myNotifier()
		{
#if 0
	not to be called here, because this destructor
	will be called after HDF5 has already been destructed
	which would lead to errors if we do the same here.

			puts("~myNotifier()"); fflush(stdout);
			H5Tclose( string_id );
			H5Gclose( script_id );
#endif
		}


		struct	H5String
		{
			hid_t	tid;

			H5String(const string&s)
			{
				tid = H5Tcopy(H5T_C_S1); 
//				H5Tset_strpad( tid, H5T_STR_NULLTERM); 
//				H5Tset_size( tid, s.length()+1 ); 
				H5Tset_size( tid, H5T_VARIABLE);
			}

			~H5String()
			{
				H5Tclose(tid);
			}
		};

		struct	Entry
		{
			hid_t	Entry_id;

			Entry(myNotifier*N, hid_t ActionType_id)
			{
			char	entry[1024];
				sprintf(entry, "%04u", N->ExecID); 
				N->ExecID++; 

			hid_t	space_id = H5Screate( H5S_SCALAR );
				Entry_id = H5Acreate2( N->script_id, entry,
						       ActionType_id, space_id,
						       H5P_DEFAULT, H5P_DEFAULT); 

				H5Sclose(space_id);
			}

			~Entry()
			{
				H5Fflush(Entry_id, H5F_SCOPE_LOCAL);
				H5Aclose(Entry_id);
			}
		};

		void createVObject(const RefPtr<VObject>&vobj, const Intercube&CreationContext,
				   const WeakPtr<VCreatorBase>&crec) override
		{
		const string
			CreatorName = crec->Name(),
			ObjectName = vobj->Name();

		H5String Crec( CreatorName ),
			 Obj ( ObjectName ); 

		hid_t	ActionType_id = H5Tcreate( H5T_COMPOUND, 2*sizeof(char* ) ); 
//						   CreatorName.length()  + 1 
//						   + ObjectName.length() + 1); 

			H5Tinsert( ActionType_id, "Creator"   , 0, Crec.tid); 
			H5Tinsert( ActionType_id, "ObjectName", sizeof(char*), Obj.tid); 
//				   CreatorName.length()+1, Obj.tid); 

		Entry	Line(this, ActionType_id); 

//		string	buf = CreatorName + '\0' + ObjectName; 
//			H5Awrite(Line.Entry_id, ActionType_id, buf.c_str() ); 

		const char*buf[2] = { CreatorName.c_str(), ObjectName.c_str() };
			H5Awrite(Line.Entry_id, ActionType_id, buf );

			H5Tclose( ActionType_id );
		}

		/**
		   The attach() function is called whenever a parameter
		   is attached to the slot of an object. It is called
		   just <b>before</b> this attachment takes place.
		   @param DestSlot	The slot within object VObjDst whose parameter will be modified.
		   @param ProvidedParam The parameter that will replace the one in the slot
		*/
		bool attachPrefix(const RefPtr<VParameter>&ProvidedParam,
				  const RefPtr<VSlot>&DestSlot)
		{
			return true;
		}
		
		struct	ParamChange
		{
			string	Object, Parameter, Value, Context;

			bool operator==(const ParamChange&P) const
			{
				return Object == P.Object &&
					Parameter == P.Parameter &&
					Value == P.Value &&
					Context == P.Context;
			}
		};

		ParamChange	LastChange;

		void	changeParameter(const WeakPtr<VParameter>&ModifiedParam,
					const RefPtr<ValuePool>&Context,
					const WeakPtr<VValueBase>&NewValue,
					const string&member) override
		{
			//printf(" $$$$$$$$$ changeParameter [%s], member: %s\n", ModifiedParam->Name().c_str(), member.c_str());
			ParamChange	Action;
			if (RefPtr<VObject> SrcObj = ModifiedParam->Source() )
				Action.Object = SrcObj->Name();

			Action.Parameter = ModifiedParam->Name(); 
			Action.Value = NewValue->Text(); 
			if (Context)
				Action.Context = Context->Name(); 

			if (Action == LastChange)
				return;

			LastChange = Action;

			if (RefPtr<VObject> SrcObj = ModifiedParam->Source() )
			{
			hid_t	ActionType_id = H5Tcreate( H5T_COMPOUND, 3*sizeof(char* ) ); 
				H5Tinsert( ActionType_id, "Object"   , 0, string_id); 
				H5Tinsert( ActionType_id, "Parameter", sizeof(char*), string_id); 
				H5Tinsert( ActionType_id, "Value"    , 2*sizeof(char*), string_id); 

			Entry	Line(this, ActionType_id); 

			const	char*buf[] = { SrcObj->Name().c_str(),
					       ModifiedParam->Name().c_str(),
					       NewValue->Text().c_str() }; 

				H5Awrite(Line.Entry_id, ActionType_id, buf ); 
				H5Tclose( ActionType_id );
			} 
			else
			{
			hid_t	ActionType_id = H5Tcreate( H5T_COMPOUND, 2*sizeof(char* ) );
				H5Tinsert( ActionType_id, "Parameter", 0*sizeof(char*), string_id); 
				H5Tinsert( ActionType_id, "Value"    , 1*sizeof(char*), string_id); 

			Entry	Line(this, ActionType_id); 

			const	char*buf[] = { ModifiedParam->Name().c_str(),
					       NewValue->Text().c_str() }; 

				H5Awrite(Line.Entry_id, ActionType_id, buf ); 
				H5Tclose( ActionType_id );
			}
// 
//			NewValue->getType(); 
//			NewValue->nComponents(); 
			//NewValue->iterate();
		}
	};


namespace
{

struct	MySaver : Wizt::VishSaver
{
	MySaver()
	: Wizt::VishSaver("v5")
	{}

	~MySaver()
	{}

	bool save(const std::string&s) override
	{
		return true;
	}

};
}

VActivityTracer::VActivityTracer(const string&name, int N, const RefPtr<VCreationPreferences>&VP)
: VObject(name, N, VP)
{
	MyNotifier = new myNotifier(this);
	MyNotifier->activate(); 

	addSaver( "HDF5 Vish script", new MySaver() );
}

VActivityTracer::~VActivityTracer()
{
}

static	Ref<VCreator<VActivityTracer> >
	MyCreator("Tools/ActivityTracer", ObjectQuality::EXPERIMENTAL);

} // namespace Wizt
