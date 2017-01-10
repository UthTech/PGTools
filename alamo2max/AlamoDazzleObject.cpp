/*
	AlamoDazzleObject.cpp
	created: 10:31am March 17,2003
	author: Greg Hjelstrom

	This is a helper object in Max which we use to indicate that an external object
	should be attached to this model at this transform.
*/

//#include <windows.h>
#include "AlamoDazzleObject.h"
#include <maxapi.h>
#include <bmmlib.h>
#include <bitmap.h>
#include <algorithm>
using namespace std;

extern HINSTANCE g_hModule;

//------------------------------------------------------
static BOOL creating = FALSE;

std::string Build_Uppercase_String(std::string str)
{
    transform(str.begin(), str.end(), str.begin(), toupper);
    return str;
}

TSTR GetString(UINT id)
{
    TCHAR buffer[128];
    LoadString(g_hModule, id, buffer, 128);
    return buffer;
}

class AlamoDazzleClassDesc : public ClassDesc 
{
public:
	int 			IsPublic() { return 1; }
	void *			Create(BOOL loading = FALSE) { return new AlamoDazzleObject; }
	const TCHAR *	ClassName() { return GetString(IDS_ALAMO_DAZZLE_CLASS); }
	SClass_ID		SuperClassID() { return HELPER_CLASS_ID; }
	Class_ID 		ClassID() { return AlamoDazzleID; }
	const TCHAR* 	Category() { return _T("");  }
	void            ResetClassParams(BOOL fileReset) { }
	
    int BeginCreate(Interface *i)	
	{ 
        SuspendSetKeyMode();
        creating = TRUE;
        return ClassDesc::BeginCreate(i); 
	}
	
    int EndCreate(Interface *i)	
	{ 
        ResumeSetKeyMode();
        creating = FALSE; 
        return ClassDesc::EndCreate(i); 
	}
};

static AlamoDazzleClassDesc AlamoDazzleDesc;
ClassDesc * Get_Alamo_Dazzle_Desc(void)
{
    return &AlamoDazzleDesc;
}

// class variable for measuring tape class.
bool AlamoDazzleObject::meshBuilt = false;
Mesh AlamoDazzleObject::meshTemplate;
HWND AlamoDazzleObject::hAlamoDazzleParams = NULL;
IObjParam *AlamoDazzleObject::iObjParams;

INT_PTR CALLBACK AlamoDazzleParamDialogProc( HWND hDlg, UINT message, WPARAM wParam, LPARAM lParam )
{
	AlamoDazzleObject *ph = (AlamoDazzleObject *)(LONG_PTR)GetWindowLongPtr( hDlg, GWLP_USERDATA );	
	if ( !ph && message != WM_INITDIALOG ) return FALSE;

	switch ( message ) {
		case WM_INITDIALOG:
			ph = (AlamoDazzleObject *)lParam;
			SetWindowLongPtr( hDlg, GWLP_USERDATA, (LONG_PTR)ph );
			SetDlgFont( hDlg, ph->iObjParams->GetAppHFont() );
			return FALSE;			

		case WM_DESTROY:
			return FALSE;

		case WM_MOUSEACTIVATE:
			ph->iObjParams->RealizeParamPanel();
			return FALSE;

		case WM_LBUTTONDOWN:			
			return FALSE;
		case WM_LBUTTONUP:			
			return FALSE;
		case WM_MOUSEMOVE:
			ph->iObjParams->RollupMouseMessage(hDlg,message,wParam,lParam);
			return FALSE;		
		case WM_COMMAND:
			switch (LOWORD(wParam)) { // Switch on ID
				case IDC_DAZZLE_TEXTUREPICK_BUTTON:
				{
					switch (HIWORD(wParam)) { // Notification codes
						case BN_BUTTONUP:  // Button is pressed.
							ph->PickTexture();
						break;
						default:
							ph->Update_Variables();
							break;
					}
				}
				break;
			}
			return FALSE;

		default:
			return FALSE;
	}
}



void AlamoDazzleObject::BeginEditParams( IObjParam *ip, ULONG flags,Animatable *prev )
{
	iObjParams = ip;
	editting = TRUE;
	
	if ( !hAlamoDazzleParams ) {
		hAlamoDazzleParams = ip->AddRollupPage( 
				g_hModule,
				MAKEINTRESOURCE(IDD_ALAMO_DAZZLE_DIALOG),
				AlamoDazzleParamDialogProc, 
				GetString(IDS_ALAMO_DAZZLE), 
				(LPARAM)this );	
		ip->RegisterDlgWnd(hAlamoDazzleParams);
	} 
	else {
		SetWindowLongPtr( hAlamoDazzleParams, GWLP_USERDATA, (LONG_PTR)this );
	}
	UpdateUI(ip->GetTime());
	Update_Controls();
}

void AlamoDazzleObject::Update_Controls(void)
{
	m_radiusSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_RADIUS_SPIN));
	if (m_radiusSpin) {
		m_radiusSpin->SetLimits(0.0f, 100.0f, FALSE);
		m_radiusSpin->SetScale(1);
		m_radiusSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_RADIUS_EDIT), EDITTYPE_FLOAT);
		m_radiusSpin->SetValue(m_radius,FALSE);
		m_radiusSpin->Enable(TRUE);
		ReleaseISpinner(m_radiusSpin);
	}
	m_colorSwatch = GetIColorSwatch(GetDlgItem(hAlamoDazzleParams, IDC_DAZZLE_COLOR));
	if (m_colorSwatch) {
		m_colorSwatch->SetColor(RGB(m_red, m_grn, m_blu), FALSE);
		m_colorSwatch->Enable(TRUE);
		ReleaseIColorSwatch(m_colorSwatch);
	}
	m_textureEdit = GetICustEdit(GetDlgItem(hAlamoDazzleParams, IDC_DAZZLE_TEXTURE_EDIT));
	if (m_textureEdit) {
		m_textureEdit->SetText(const_cast<TCHAR*>(m_textureName.c_str()));
		m_textureEdit->Enable(TRUE);
		ReleaseICustEdit(m_textureEdit);
	}
	m_slotxSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTX_SPIN));
	if (m_slotxSpin) {
		m_slotxSpin->SetLimits(0, 15, FALSE);
		m_slotxSpin->SetScale(1);
		m_slotxSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTX_EDIT), EDITTYPE_INT);
		m_slotxSpin->SetValue(m_slotx,FALSE);
		m_slotxSpin->Enable(TRUE);
		ReleaseISpinner(m_slotxSpin);
	}
	m_slotySpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTY_SPIN));
	if (m_slotySpin) {
		m_slotySpin->SetLimits(0, 15, FALSE);
		m_slotySpin->SetScale(1);
		m_slotySpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTY_EDIT), EDITTYPE_INT);
		m_slotySpin->SetValue(m_sloty,FALSE);
		m_slotySpin->Enable(TRUE);
		ReleaseISpinner(m_slotySpin);
	}
	m_numslotsSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_NUMSLOTS_SPIN));
	if (m_numslotsSpin) {
		m_numslotsSpin->SetLimits(0, 16, FALSE);
		m_numslotsSpin->SetScale(1);
		m_numslotsSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_NUMSLOTS_EDIT), EDITTYPE_INT);
		m_numslotsSpin->SetValue(m_numSlots,FALSE);
		m_numslotsSpin->Enable(TRUE);
		ReleaseISpinner(m_numslotsSpin);
	}
	m_blinkFreqSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_FREQ_SPIN));
	if (m_blinkFreqSpin) {
		m_blinkFreqSpin->SetLimits(0.0f, 30.0f, FALSE);
		m_blinkFreqSpin->SetScale(1);
		m_blinkFreqSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_FREQ_EDIT), EDITTYPE_FLOAT);
		m_blinkFreqSpin->SetValue(m_blinkFreq,FALSE);
		m_blinkFreqSpin->Enable(TRUE);
		ReleaseISpinner(m_blinkFreqSpin);
	}
	m_blinkPhaseSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_PHASE_SPIN));
	if (m_blinkPhaseSpin) {
		m_blinkPhaseSpin->SetLimits(0.0f, 30.0f, FALSE);
		m_blinkPhaseSpin->SetScale(0.1f);
		m_blinkPhaseSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_PHASE_EDIT), EDITTYPE_FLOAT);
		m_blinkPhaseSpin->SetValue(m_blinkPhase,FALSE);
		m_blinkPhaseSpin->Enable(TRUE);
		ReleaseISpinner(m_blinkPhaseSpin);
	}
	m_blinkBiasSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_BIAS_SPIN));
	if (m_blinkBiasSpin) {
		m_blinkBiasSpin->SetLimits(0.0f, 1.0f, FALSE);
		m_blinkBiasSpin->SetScale(0.1f);
		m_blinkBiasSpin->LinkToEdit(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_BIAS_EDIT), EDITTYPE_FLOAT);
		m_blinkBiasSpin->SetValue(m_blinkBias,FALSE);
		m_blinkBiasSpin->Enable(TRUE);
		ReleaseISpinner(m_blinkBiasSpin);
	}
	m_nightOnlyButton = GetICustButton(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_NIGHT_BUTTON));
	if (m_nightOnlyButton) {
		m_nightOnlyButton->SetType(CBT_CHECK);
		m_nightOnlyButton->SetCheck(m_nightOnly ? TRUE : FALSE);
		m_nightOnlyButton->SetText("Night Only");		
		m_nightOnlyButton->Enable(TRUE);
		ReleaseICustButton(m_nightOnlyButton);
	}
	m_texturePickButton = GetICustButton(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_TEXTUREPICK_BUTTON));
	if (m_texturePickButton) {
		m_texturePickButton->SetText("Pick Texture");		
		m_texturePickButton->Enable(TRUE);
      m_texturePickButton->SetButtonDownNotify(TRUE);
		ReleaseICustButton(m_texturePickButton);
	}
}

void AlamoDazzleObject::Update_Variables(void)
{
	m_radiusSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_RADIUS_SPIN));
	if (m_radiusSpin) {
		m_radius = m_radiusSpin->GetFVal();
		ReleaseISpinner(m_radiusSpin);
	}
	m_colorSwatch = GetIColorSwatch(GetDlgItem(hAlamoDazzleParams, IDC_DAZZLE_COLOR));
	if (m_colorSwatch) {
		COLORREF c;
		c = m_colorSwatch->GetColor();
		m_red = GetRValue(c);
		m_grn = GetGValue(c);
		m_blu = GetBValue(c);
		ReleaseIColorSwatch(m_colorSwatch);
	}
	m_textureEdit = GetICustEdit(GetDlgItem(hAlamoDazzleParams, IDC_DAZZLE_TEXTURE_EDIT));
	if (m_textureEdit) {
		char tmp[256];
		m_textureEdit->GetText(tmp, 256);
		std::string foo = tmp;
		m_textureName = Build_Uppercase_String(foo);
		ReleaseICustEdit(m_textureEdit);
	}
	m_slotxSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTX_SPIN));
	if (m_slotxSpin) {
		m_slotx = m_slotxSpin->GetIVal();
		ReleaseISpinner(m_slotxSpin);
	}
	m_slotySpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_SLOTY_SPIN));
	if (m_slotySpin) {
		m_sloty = m_slotySpin->GetIVal();
		ReleaseISpinner(m_slotySpin);
	}
	m_numslotsSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_NUMSLOTS_SPIN));
	if (m_numslotsSpin) {
		m_numSlots = m_numslotsSpin->GetIVal();
		ReleaseISpinner(m_numslotsSpin);
	}
	m_blinkFreqSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_FREQ_SPIN));
	if (m_blinkFreqSpin) {
		m_blinkFreq = m_blinkFreqSpin->GetFVal();
		ReleaseISpinner(m_blinkFreqSpin);
	}
	m_blinkPhaseSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_PHASE_SPIN));
	if (m_blinkPhaseSpin) {
		m_blinkPhase = m_blinkPhaseSpin->GetFVal();
		ReleaseISpinner(m_blinkPhaseSpin);
	}
	m_blinkBiasSpin = GetISpinner(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_BIAS_SPIN));
	if (m_blinkBiasSpin) {
		m_blinkBias = m_blinkBiasSpin->GetFVal();
		ReleaseISpinner(m_blinkBiasSpin);
	}
	m_nightOnlyButton = GetICustButton(GetDlgItem(hAlamoDazzleParams,IDC_DAZZLE_NIGHT_BUTTON));
	if (m_nightOnlyButton) {
		m_nightOnly = m_nightOnlyButton->IsChecked() == TRUE;
		ReleaseICustButton(m_nightOnlyButton);
	}
}
		
void AlamoDazzleObject::EndEditParams( IObjParam *ip, ULONG flags,Animatable *prev)
{
	editting = FALSE;
	Update_Variables();
	BuildMesh();	

	if ( flags&END_EDIT_REMOVEUI ) {		
		ip->UnRegisterDlgWnd(hAlamoDazzleParams);
		ip->DeleteRollupPage(hAlamoDazzleParams);
		hAlamoDazzleParams = NULL;				
	} 
	else {
		SetWindowLongPtr( hAlamoDazzleParams, GWLP_USERDATA, 0 );
	}
	iObjParams = NULL;
}


void AlamoDazzleObject::BuildMesh()
{
	int nverts = 8;
	int nfaces = 12;

	if (!meshBuilt) {		
		meshTemplate.setNumVerts(nverts);
		meshTemplate.setNumFaces(nfaces);
	
		meshTemplate.setVert(0, Point3(+1.0, +1.0, +1.0));
		meshTemplate.setVert(1, Point3(-1.0, +1.0, +1.0));
		meshTemplate.setVert(2, Point3(-1.0, -1.0, +1.0));
		meshTemplate.setVert(3, Point3(+1.0, -1.0, +1.0));
		meshTemplate.setVert(4, Point3(+1.0, +1.0, -1.0));
		meshTemplate.setVert(5, Point3(-1.0, +1.0, -1.0));
		meshTemplate.setVert(6, Point3(-1.0, -1.0, -1.0));
		meshTemplate.setVert(7, Point3(+1.0, -1.0, -1.0));

		int i = 0;	
		// top and bottom
		meshTemplate.faces[i].setVerts(0 ,1 ,2 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(0 ,2 ,3 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(4 ,6 ,5 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(4 ,7 ,6 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;

		// left and right
		meshTemplate.faces[i].setVerts(0 ,3 ,4 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(4 ,3 ,7 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(5 ,2 ,1 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(5 ,6 ,2 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;

		// front and back
		meshTemplate.faces[i].setVerts(0 ,5 ,1 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(0 ,4 ,5 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(3 ,2 ,6 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;
		meshTemplate.faces[i].setVerts(3 ,6 ,7 ); meshTemplate.faces[i].setEdgeVisFlags(1 ,1 ,0 ); i++;

		meshBuilt = true;
	}


	if (m_radius == m_displayRadius) {
		return;
	}
	m_displayRadius = m_radius;
	mesh = meshTemplate;

	// adjust the size of the mesh
	Matrix3 mat;
	mat.IdentityMatrix();
	mat.Scale(Point3(m_radius,m_radius,m_radius));	
	for (int i=0; i<nverts; i++)
		mesh.getVert(i) = mat*mesh.getVert(i);
}

void AlamoDazzleObject::UpdateUI(TimeValue t)
{
}


AlamoDazzleObject::AlamoDazzleObject() : HelperObject(),
	m_radius(1.0f),
	m_displayRadius(-1.0f),
	m_radiusSpin(NULL),
	m_colorSwatch(NULL),
	m_textureEdit(NULL),
	m_red(100),
	m_grn(100),
	m_blu(100),
	m_textureName("LENSFLARE.TGA"),
	m_slotx(0),
	m_sloty(0),
	m_numSlots(2),
	m_blinkFreq(1.0f),
	m_blinkPhase(0.0f),
	m_blinkBias(0.5f),
	m_nightOnly(false)
{
	editting = 0;
	created = 0;
	suspendSnap = FALSE;
	ivalid.SetEmpty();
	BuildMesh();
	m_blinkPhase = (rand() % 30) / 30.0f;
}

AlamoDazzleObject::~AlamoDazzleObject()
{
	DeleteAllRefsFromMe();
}


int AlamoDazzleObject::NumRefs()
{
	return 2;
}

RefTargetHandle AlamoDazzleObject::GetReference(int i)
{
	return NULL;
}

void AlamoDazzleObject::SetReference(int i, RefTargetHandle rtarg)
{
}

class AlamoDazzleObjCreateCallBack: public CreateMouseCallBack 
{
	AlamoDazzleObject *ph;
public:
	int proc( ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat );
	void SetObj(AlamoDazzleObject *obj) { ph = obj; }
};

int AlamoDazzleObjCreateCallBack::proc(ViewExp *vpt,int msg, int point, int flags, IPoint2 m, Matrix3& mat ) 
{	
	if (msg==MOUSE_POINT||msg==MOUSE_MOVE) {
		switch(point) {
		case 0:
			ph->suspendSnap = TRUE;
			mat.SetTrans(vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
			break;
		case 1:
			mat.SetTrans(vpt->SnapPoint(m,m,NULL,SNAP_IN_PLANE));
			if (msg==MOUSE_POINT) {
				ph->suspendSnap = FALSE;
				return 0;
			}
			break;			
		}
	} else if (msg == MOUSE_ABORT) {		
		return CREATE_ABORT;
	}
	return 1;
}

static AlamoDazzleObjCreateCallBack AlamoDazzleCreateCB;

CreateMouseCallBack* AlamoDazzleObject::GetCreateMouseCallBack() 
{
	AlamoDazzleCreateCB.SetObj(this);
	return &AlamoDazzleCreateCB;
}

void AlamoDazzleObject::GetMat(TimeValue t, INode* inode, ViewExp* vpt, Matrix3& tm) 
{
	tm = inode->GetObjectTM(t);	
}

void AlamoDazzleObject::GetDeformBBox(TimeValue t, Box3& box, Matrix3 *tm, BOOL useSel )
{
	box = mesh.getBoundingBox(tm);
}

void AlamoDazzleObject::GetLocalBoundBox(TimeValue t, INode* inode, ViewExp* vpt, Box3& box ) 
{
	box = mesh.getBoundingBox();	
}

void AlamoDazzleObject::GetWorldBoundBox(TimeValue t, INode* inode, ViewExp* vpt, Box3& box )
{
	int i, nv;
	Matrix3 tm;
	Point3 pt;
	GetMat(t,inode,vpt,tm);
	nv = mesh.getNumVerts();
	box.Init();
	for (i=0; i<nv; i++) 
		box += tm*mesh.getVert(i);
}

// From BaseObject

int AlamoDazzleObject::HitTest(TimeValue t, INode *inode, int type, int crossing, int flags, IPoint2 *p, ViewExp *vpt) 
{
	HitRegion hitRegion;
	DWORD	savedLimits;
	Matrix3 m;
	GraphicsWindow *gw = vpt->getGW();	
	Material *mtl = gw->getMaterial();
	MakeHitRegion(hitRegion,type,crossing,4,p);	
	gw->setRndLimits(((savedLimits = gw->getRndLimits()) | GW_PICK) & ~GW_ILLUM);
	GetMat(t,inode,vpt,m);
	gw->setTransform(m);
	// if we get a hit on the mesh, we're done
	gw->clearHitCode();
	if (mesh.select( gw, mtl, &hitRegion, flags & HIT_ABORTONHIT )) 
		return TRUE;

	return FALSE;
}

void AlamoDazzleObject::Snap(TimeValue t, INode* inode, SnapInfo *snap, IPoint2 *p, ViewExp *vpt) 
{
	if(suspendSnap)
		return;

	Matrix3 tm = inode->GetObjectTM(t);	
	GraphicsWindow *gw = vpt->getGW();	
	gw->setTransform(tm);

	Matrix3 invPlane = Inverse(snap->plane);

	// Make sure the vertex priority is active and at least as important as the best snap so far
	if(snap->vertPriority > 0 && snap->vertPriority <= snap->priority) {
		Point2 fp = Point2((float)p->x, (float)p->y);
		Point2 screen2;
		IPoint3 pt3;

		Point3 thePoint(0,0,0);
		// If constrained to the plane, make sure this point is in it!
		if(snap->snapType == SNAP_2D || snap->flags & SNAP_IN_PLANE) {
			Point3 test = thePoint * tm * invPlane;
			if(fabs(test.z) > 0.0001)	// Is it in the plane (within reason)?
				return;
		}
		gw->wTransPoint(&thePoint,&pt3);
		screen2.x = (float)pt3.x;
		screen2.y = (float)pt3.y;

		// Are we within the snap radius?
		int len = (int)Length(screen2 - fp);
		if(len <= snap->strength) {
			// Is this priority better than the best so far?
			if(snap->vertPriority < snap->priority) {
				snap->priority = snap->vertPriority;
				snap->bestWorld = thePoint * tm;
				snap->bestScreen = screen2;
				snap->bestDist = len;
			}
			else if(len < snap->bestDist) {
				snap->priority = snap->vertPriority;
				snap->bestWorld = thePoint * tm;
				snap->bestScreen = screen2;
				snap->bestDist = len;
			}
		}
	}
}

int AlamoDazzleObject::Display(TimeValue t, INode* inode, ViewExp *vpt, int flags) 
{
	Matrix3 m;
	GraphicsWindow *gw = vpt->getGW();
	Material *mtl = gw->getMaterial();

	BuildMesh();

	created = TRUE;
	GetMat(t,inode,vpt,m);
	gw->setTransform(m);
	DWORD rlim = gw->getRndLimits();
	gw->setRndLimits(GW_WIREFRAME|GW_EDGES_ONLY|GW_BACKCULL);
	if (inode->Selected()) 
		gw->setColor( LINE_COLOR, GetSelColor());
	else if(!inode->IsFrozen() && !inode->Dependent())
		gw->setColor( LINE_COLOR, GetUIColor(COLOR_TAPE_OBJ));
	mesh.render( gw, mtl, NULL, COMP_ALL);
	
	return(0);
}

void AlamoDazzleObject::FreeCaches()
{
	m_displayRadius = -1;
}

// From GeomObject
int AlamoDazzleObject::IntersectRay(TimeValue t, Ray& r, float& at) { return(0); }

//
// Reference Managment:
//

// This is only called if the object MAKES references to other things.
RefResult AlamoDazzleObject::NotifyRefChanged(Interval changeInt, RefTargetHandle hTarget, 
     PartID& partID, RefMessage message ) 
{
	return REF_SUCCEED;
}

ObjectState AlamoDazzleObject::Eval(TimeValue time)
{
	return ObjectState(this);
}

void AlamoDazzleObject::Invalidate()
{
	ivalid.SetEmpty();
}

Interval AlamoDazzleObject::ObjectValidity(TimeValue t) 
{
	ivalid = Interval(t,t);
	return ivalid;
}

RefTargetHandle AlamoDazzleObject::Clone(RemapDir& remap) 
{
	AlamoDazzleObject* newob = new AlamoDazzleObject();
	BaseClone(this, newob, remap);
	newob->m_radius = m_radius;
	newob->m_red = m_red;
	newob->m_grn = m_grn;
	newob->m_blu = m_blu;
	newob->m_textureName = m_textureName;
	newob->m_slotx = m_slotx;
	newob->m_sloty = m_sloty;
	newob->m_numSlots = m_numSlots;
	newob->m_blinkFreq = m_blinkFreq;
	newob->m_blinkPhase = m_blinkPhase;
	newob->m_nightOnly = m_nightOnly;	
	newob->m_blinkBias = m_blinkBias;
	
	return(newob);
}

enum {
	RADIUS_CHUNK = 0,
	COLOR_CHUNK,
	TEXTURE_CHUNK,
	SLOTS_CHUNK,
	BLINK_CHUNK,
	NIGHT_CHUNK,
	BIAS_CHUNK
};

// IO
IOResult AlamoDazzleObject::Save(ISave *isave) 
{
	if(editting)
	{
		Update_Variables();
	}
	ULONG nb; 
   isave->BeginChunk(RADIUS_CHUNK); 
   isave->Write(&m_radius, sizeof(float), &nb); 
   isave->EndChunk(); 

	isave->BeginChunk(COLOR_CHUNK); 
   isave->Write(&m_red, sizeof(char), &nb); 
	isave->Write(&m_grn, sizeof(char), &nb); 
	isave->Write(&m_blu, sizeof(char), &nb); 
   isave->EndChunk(); 

	isave->BeginChunk(TEXTURE_CHUNK); 
	isave->WriteCString(m_textureName.c_str());
   isave->EndChunk(); 

	isave->BeginChunk(SLOTS_CHUNK); 
	isave->Write(&m_slotx, sizeof(int), &nb);
	isave->Write(&m_sloty, sizeof(int), &nb);
	isave->Write(&m_numSlots, sizeof(int), &nb);
   isave->EndChunk(); 

	isave->BeginChunk(BLINK_CHUNK); 
	isave->Write(&m_blinkFreq, sizeof(float), &nb);
	isave->Write(&m_blinkPhase, sizeof(float), &nb);
   isave->EndChunk(); 

	isave->BeginChunk(BIAS_CHUNK);
	isave->Write(&m_blinkBias, sizeof(float), &nb);
	isave->EndChunk(); 

	isave->BeginChunk(NIGHT_CHUNK); 
	isave->Write(&m_nightOnly, sizeof(bool), &nb);
	isave->EndChunk(); 

   return IO_OK; 

}

IOResult AlamoDazzleObject::Load(ILoad *iload) 
{
	ULONG nb; 
   IOResult res; 
	char *str;
   while (IO_OK==(res=iload->OpenChunk())) { 
     switch(iload->CurChunkID()) { 
       case RADIUS_CHUNK: 
         res=iload->Read(&m_radius, sizeof(float), &nb); 
         break; 
		case COLOR_CHUNK: 
         res=iload->Read(&m_red, sizeof(char), &nb); 
			res=iload->Read(&m_grn, sizeof(char), &nb); 
			res=iload->Read(&m_blu, sizeof(char), &nb); 
         break; 
	  case BIAS_CHUNK:
		  res=iload->Read(&m_blinkBias, sizeof(float), &nb); 
		  break;
		case TEXTURE_CHUNK:
			res=iload->ReadCStringChunk(&str);
			m_textureName = str;
			break;
	  case SLOTS_CHUNK: 
         res=iload->Read(&m_slotx, sizeof(int), &nb); 
			res=iload->Read(&m_sloty, sizeof(int), &nb); 
			res=iload->Read(&m_numSlots, sizeof(int), &nb); 
         break; 
		case BLINK_CHUNK: 
         res=iload->Read(&m_blinkFreq, sizeof(float), &nb); 
			res=iload->Read(&m_blinkPhase, sizeof(float), &nb); 
			break;
	  case NIGHT_CHUNK:
		  res=iload->Read(&m_nightOnly, sizeof(bool), &nb); 
		  break;
     } 
     iload->CloseChunk(); 
     if (res!=IO_OK) return res; 
   }
   return IO_OK; 
}

// Animatable methods
void AlamoDazzleObject::DeleteThis() 
{ 
	delete this; 
}

Class_ID AlamoDazzleObject::ClassID() 
{ 
	return AlamoDazzleID;
}  

void AlamoDazzleObject::GetClassName(TSTR& s) 
{ 
	s = TSTR(GetString(IDS_ALAMO_DAZZLE_CLASS)); 
}

TSTR AlamoDazzleObject::SubAnimName(int i) 
{ 
	return TSTR(GetString(IDS_ALAMO_DAZZLE));
}

void AlamoDazzleObject::PickTexture()
{
	BitmapInfo bi;		
	if (TheManager->SelectFileInput(&bi, NULL, "Texture Name")) {
		std::string foo = bi.Filename();
		m_textureName = Build_Uppercase_String(foo);
		m_textureEdit = GetICustEdit(GetDlgItem(hAlamoDazzleParams, IDC_DAZZLE_TEXTURE_EDIT));
		if (m_textureEdit) {
			m_textureEdit->SetText(const_cast<TCHAR*>(m_textureName.c_str()));
			m_textureEdit->Enable(TRUE);
			ReleaseICustEdit(m_textureEdit);
		}
	}
}
