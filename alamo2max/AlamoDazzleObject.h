/*
	alamoproxyobject.h
	created: 10:31am March 17,2003
	author: Greg Hjelstrom

	This is a helper object in Max which we use to indicate that an external object
	should be attached to this model at this transform.
*/
#ifndef ALAMODAZZLEOBJECT_H
#define ALAMODAZZLEOBJECT_H

#include <Max.h>
#if MAX_RELEASE >= MAX_RELEASE_R9
#include <MaxHeapDirect.h>
#else
#include <MAX_MemDirect.h>
#endif
#include "resource.h"
#include <string>

#if MAX_RELEASE != MAX_RELEASE_R9
#define DefaultRemapDir NoRemap
#endif

void* ::operator new(size_t size);
void ::operator delete(void* p);

extern TSTR GetString(UINT id);
extern ClassDesc * Get_Alamo_Dazzle_Desc(void);
static const Class_ID AlamoDazzleID = Class_ID(0xb187920, 0x47e32d75);

class AlamoDazzleObject: public HelperObject 
{
	friend class AlamoDazzleObjCreateCallBack;
	friend INT_PTR CALLBACK AlamoDazzleParamDialogProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam );
	friend void resetProtParams();
public:

	// Class vars
	static Mesh       meshTemplate;
	static bool       meshBuilt;
	static HWND       hAlamoDazzleParams;
	static IObjParam *iObjParams;
	BOOL              created;

	// for display purposese
	Mesh 			  mesh;
	float			  m_displayRadius;

	// dazzle variables
	float			  m_radius;
	unsigned char	  m_red;
	unsigned char	  m_grn;
	unsigned char	  m_blu;
    std::string       m_textureName;	
	int				  m_slotx;
	int				  m_sloty;
	int				  m_numSlots;
	float			  m_blinkFreq;
	float			  m_blinkPhase;
	float			  m_blinkBias;
	bool			  m_nightOnly;

	// dazzle controls
    ISpinnerControl  *m_radiusSpin;
	IColorSwatch	 *m_colorSwatch;
	ICustEdit		 *m_textureEdit;
	ISpinnerControl  *m_slotxSpin;
	ISpinnerControl  *m_slotySpin;
	ISpinnerControl  *m_numslotsSpin;
	ISpinnerControl  *m_blinkFreqSpin;
	ISpinnerControl  *m_blinkPhaseSpin;
	ISpinnerControl  *m_blinkBiasSpin;
	ICustButton		 *m_nightOnlyButton;
	ICustButton		 *m_texturePickButton;

	void Update_Controls(void);
	void Update_Variables(void);
	void PickTexture();

	// Snap suspension flag (TRUE during creation only)
	BOOL suspendSnap;

	// Object parameters		
	short editting;
	Interval ivalid;

	//  inherited virtual methods for Reference-management
	RefResult NotifyRefChanged( Interval changeInt, RefTargetHandle hTarget, 
	   PartID& partID, RefMessage message );
	void BuildMesh();
	void UpdateUI(TimeValue t);
	void GetMat(TimeValue t, INode* inod, ViewExp *vpt, Matrix3& mat);
	BOOL GetTargetPoint(int which, TimeValue t, Point3* q);
	int DrawLines(TimeValue t, INode *inode, GraphicsWindow *gw, int drawing );

	AlamoDazzleObject();
	~AlamoDazzleObject();

	//  inherited virtual methods:

	// From BaseObject
	int HitTest(TimeValue t, INode* inode, int type, int crossing, int flags, IPoint2 *p, ViewExp *vpt);
	void Snap(TimeValue t, INode* inode, SnapInfo *snap, IPoint2 *p, ViewExp *vpt);
	int Display(TimeValue t, INode* inode, ViewExp *vpt, int flags);
	CreateMouseCallBack* GetCreateMouseCallBack();
	void BeginEditParams( IObjParam *ip, ULONG flags,Animatable *prev);
	void EndEditParams( IObjParam *ip, ULONG flags,Animatable *next);
	const TCHAR *GetObjectName() { return GetString(IDS_ALAMO_DAZZLE); }

	// From Object
	ObjectState Eval(TimeValue time);
	void InitNodeName(TSTR& s) { s = GetString(IDS_ALAMO_DAZZLE); }
	Interval ObjectValidity(TimeValue time);
	void Invalidate();
	int DoOwnSelectHilite() { return 1; }

	// From GeomObject
	int IntersectRay(TimeValue t, Ray& r, float& at);
	void GetWorldBoundBox(TimeValue t, INode *mat, ViewExp *vpt, Box3& box );
	void GetLocalBoundBox(TimeValue t, INode *mat, ViewExp *vpt, Box3& box );
	void GetDeformBBox(TimeValue t, Box3& box, Matrix3 *tm, BOOL useSel );

	BOOL HasViewDependentBoundingBox() { return true; }

	// Animatable methods
	void DeleteThis();
	Class_ID ClassID();
	void GetClassName(TSTR& s);
	TSTR SubAnimName(int i);

	void FreeCaches();


	// From ref
	RefTargetHandle Clone(RemapDir& remap);
	int NumRefs();
	RefTargetHandle GetReference(int i);
	void SetReference(int i, RefTargetHandle rtarg);

	// IO
	IOResult Save(ISave *isave);
	IOResult Load(ILoad *iload);
};				

#endif
