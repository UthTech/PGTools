/* Alamo Object Importer   *
 *          v1.1           *
 *    by Mike Lankamp      *
 */
 
utility AloImporter "Alamo Object Importer V1.1" (
 
	group "Texture paths"
	(
		listbox textures height:2
		button addTexturePath "+" across: 2
		button delTexturePath "-" enabled:false
	)
	
	group "Shader paths"
	(
		listbox shaders height:2
		button addShaderPath "+" across: 2
		button delShaderPath "-" enabled:false
	)
	
	button doImport "Import Alamo Object" offset:(point2 0 0)
	
	local attachmentBone = undefined
	
	rollout selectNode "Select the attachment bone"
	(
		listbox bones height:15
		button ok "OK" across: 2
		button none "None"
		
		on selectNode open do
		(
			local items
			for node in $* do
			(
				local nodes = getNodeByName node.name exact:true all:true
				if nodes.count == 1 then
				(
					items = append bones.items node.name
				)
			)
			qsort items stricmp
			bones.items = items
		)
		
		on ok pressed do
		(
			attachmentBone = undefined
			if bones.selection > 0 then
			(
				attachmentBone = getNodeByName bones.items[bones.selection] exact:true
			)
			DestroyDialog selectNode
		)

		on none pressed do
		(
			attachmentBone = undefined
			DestroyDialog selectNode
		)
	)
	
	on addTexturePath pressed do
	(
		local path = AlamoImporter.Helper.QueryPath "Please select a texture path"
		if path != undefined then
		(
			textures.items = append textures.items path
			textures.selection = textures.items.count
			delTexturePath.enabled = true
			AlamoImporter.Helper.SaveTexturePaths textures.items
		)
	)
	
	on addShaderPath pressed do
	(
		local path = AlamoImporter.Helper.QueryPath "Please select a shader path"
		if path != undefined then
		(
			shaders.items = append shaders.items path
			shaders.selection = shaders.items.count
			delShaderPath.enabled = true
			AlamoImporter.Helper.SaveShaderPaths shaders.items
		)
	)
	
	on delTexturePath pressed do if textures.items.count > 0 and textures.selection > 0 do
	(
		textures.items = deleteItem textures.items textures.selection
		AlamoImporter.Helper.SaveTexturePaths textures.items
		if textures.items.count == 0 then delTexturePath.enabled = false
	)
	
	on delShaderPath pressed  do if shaders.items.count > 0 and shaders.selection > 0 do
	(
		shaders.items = deleteItem shaders.items shaders.selection
		AlamoImporter.Helper.SaveShaderPaths shaders.items
		if shaders.items.count == 0 then delShaderPath.enabled = false
	)
	
	on textures selected index do delTexturePath.enabled = (index > 0)
	on shaders selected index do  delShaderPath.enabled  = (index > 0)

	on AloImporter open do
	(
		-- Ensure our helper class is loaded
		pluginManager.loadClass AlamoImporter
		
		-- Load the texture and shader paths
		textures.items = AlamoImporter.Helper.GetTexturePaths()
		shaders.items  = AlamoImporter.Helper.GetShaderPaths()
		delTexturePath.enabled = (textures.selection > 0)
		delShaderPath.enabled = (shaders.selection > 0)
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- A dictionary class, for O(log N) lookup
	
	struct Dictionary
	(
		keys   = #(),
		values = #(),
		
		-- Private function
		-- Finds the index of a key. Negative if not found but should be inserted there.
 		fn FindKey k =
		(
			local low  = 1
			local high = keys.count
			while high >= low do
			(
				local mid = (low + high) / 2
				if k == keys[mid] then return mid;
				if k <  keys[mid] then high = mid - 1 else low = mid + 1
			)
			return -low
		),
		
		fn HasKey k = 
		(
			return (FindKey k) > 0
		),
		
		fn Get k =
		(
			local i = FindKey k
			if i > 0 then return values[i]
		),
		
		fn Remove k = 
		(
			local i = FindKey k
			if i > 0 then
			(
				deleteItem keys   i
				deleteItem values i
			)
		),
		
		fn Put k v =
		(
			local i = FindKey k
			if i < 0 then
			(
				insertItem k keys   -i
				insertItem v values -i
			)
			else
			(
				values[i] = v
			)
		)
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- ChunkReader class, for reading chunked files
	
	struct ChunkHeader(type, size)

	struct ChunkReader
	(
		m_stream,
		m_offsets    = #(),
		m_position   =  0,
		m_size       = -1,
		m_miniOffset =  0,
		m_miniSize   = -1,

		fn Skip =
		(
			if m_miniSize >= 0 then
			(
				fseek m_stream m_miniOffset #seek_set
			)
			else
			(
				fseek m_stream (m_offsets[m_offsets.count]) #seek_set
				deleteItem m_offsets m_offsets.count
			)
		),
		
		fn NextMini =
		(
			if m_miniSize >= 0 then
			(
				-- We're in a mini chunk, so skip it
				fseek m_stream m_miniOffset #seek_set
			)
			
			if (ftell m_stream) == m_offsets[m_offsets.count] then
			(
				-- We're at the end of the current chunk, move up one
				deleteItem m_offsets m_offsets.count
				m_size     = -1
				m_position =  0
				return -1
			)
			
			local header = ChunkHeader (ReadByte m_stream #unsigned) (ReadByte m_stream #unsigned)
			m_miniSize   = header.size
			m_miniOffset = (ftell m_stream) + m_miniSize
			m_position   = 0
			return header.type
		),
		
		fn Next =
		(
			if m_size >= 0 then
			(
				-- We're in a data chunk, so skip it
				fseek m_stream (m_offsets[m_offsets.count]) #seek_set
				deleteItem m_offsets m_offsets.count
			)
			
			if (ftell m_stream) == m_offsets[m_offsets.count] then
			(
				-- We're at the end of the current chunk, move up one
				deleteItem m_offsets m_offsets.count
				m_size     = -1
				m_position =  0
				return -1
			)
			
			local header = ChunkHeader (ReadLong m_stream #unsigned) (ReadLong m_stream #unsigned)
			append m_offsets ((ftell m_stream) + (bit.and header.size 0x7FFFFFFF))
			if (bit.and header.size 0x80000000) != 0 then m_size = -1 else m_size = header.size
			m_miniSize = -1
			m_position =  0
			return header.type
		),
		
		fn Size =
		(
			if m_miniSize >= 0 then return m_miniSize else return m_size
		),
		
		fn Seek offset =
		(
			local pos = m_position + offset
			if pos < 0 then pos = 0
			else if pos > Size() then pos = Size()
			fseek m_stream (pos - m_position) #seek_cur
			m_position = pos
		),

		fn GetByte   = ( if Size() - m_position < 1 then throw "Bad or corrupt file"; m_position += 1; return ReadByte  m_stream #unsigned ),
		fn GetSShort = ( if Size() - m_position < 2 then throw "Bad or corrupt file"; m_position += 2; return ReadShort m_stream #signed ),
		fn GetShort  = ( if Size() - m_position < 2 then throw "Bad or corrupt file"; m_position += 2; return ReadShort m_stream #unsigned ),
		fn GetLong   = ( if Size() - m_position < 4 then throw "Bad or corrupt file"; m_position += 4; return ReadLong  m_stream #unsigned ),
		fn GetFloat  = ( if Size() - m_position < 4 then throw "Bad or corrupt file"; m_position += 4; return ReadFloat m_stream ),
		fn GetString =
		(
			local str = ReadString m_stream;
			if str.count + 1 < Size() - m_position then throw "Bad or corrupt file"
			m_position += str.count + 1;
			return str
		),
		
		fn Open filename =
		(
			-- Initialize filestream
			m_stream = fopen filename "rb"
			if m_stream == undefined then throw "Unable to open file"

			m_offsets = #()
			fseek m_stream 0 #seek_end
			append m_offsets (ftell m_stream)
			
			fseek m_stream 0 #seek_set
		),
		
		fn Close = 
		(
			FClose m_stream
			m_offsets = #()
			m_size    = -1
		)
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- Model reader
	
	fn Verify expr place =
	(
		if not expr then
		(
			print ("Bad or corrupt file at " + place)
			throw "Bad or corrupt file"
		)
	)
	
	-- Parameter type enumeration	
	local PT_INT     = 0
	local PT_FLOAT   = 1
	local PT_FLOAT3  = 2
	local PT_FLOAT4  = 3
	local PT_TEXTURE = 4
	
	-- Light type enumeration
	local PT_OMNI        = 0
	local PT_DIRECTIONAL = 1
	local LT_SPOTLIGHT   = 2
	
	struct BoneInfo(name, parent, visible, billboardMode, transform)
	struct ParamInfo(name, type, value)
	struct VertexInfo(pos, tex, color, alpha, bones, weights)
	struct MaterialInfo(name, params = #(), vertices = #(), faces = #(), boneMapping = #(), map = #())
	struct MeshInfo(name, bone, isHidden, isCollisionMesh, materials = #(), vweights, vbones)
	struct LightInfo(name, bone, type, color, intensity, farAttenuationEnd, farAttenuationStart, hotspotSize, falloffSize)
	struct ProxyInfo(name, bone, isHidden, altDecreaseStayHidden)
	struct DazzleInfo(name, bone, isHidden, position, color, texture, radius, phase, bias, frequency, nightOnly, slotX, slotY, numSlots)
	struct ModelInfo(bones = #(), meshes = #(), lights = #(), proxies = #(), dazzles = #())
	
	fn ReadBones reader =
	(
		Verify (reader.Next() == 0x201) "Skeleton"
		local bones = #()
		bones.count = reader.GetLong()
		for i = 1 to bones.count do
		(
			local bone = BoneInfo()
			Verify (reader.Next() == 0x202) "Bone"

			-- Read bone name
			Verify (reader.Next() == 0x203) "Bone name"
			bone.name = reader.GetString()
			
			-- Read bone data
			local type = reader.Next()
			Verify (type == 0x205 or type == 0x206) "Bone data"
			bone.parent        = reader.GetLong()
			bone.visible       = reader.GetLong() != 0
			bone.billboardMode = 0
			bone.transform     = Matrix3 1
			if bone.parent == 0 then
			(
				-- Only assign if it's not to the root bone
				bone.parent = undefined
			)
			
			if type == 0x206 then
			(
				bone.billboardMode = reader.GetLong()
			)
			
			-- Read the transform matrix
			local c = #()
			for j = 1 to 12 do
				c[j] = reader.GetFloat()
			bone.transform = Matrix3 [c[1],c[5],c[9]] [c[2],c[6],c[10]] [c[3],c[7],c[11]] [c[4],c[8],c[12]]
				
			Verify (reader.Next() == -1) "End of bone"
			bones[i] = bone
		)
		
		Verify (reader.Next() == -1) "End of skeleton"
		return bones
	)
	
	fn ReadParameters reader =
	(
		local params = #()

		-- Read material parameters
		local type = reader.Next()
		while type != -1 do
		(
			local param = ParamInfo()
			
			-- Read parameter name
			Verify (reader.nextMini() == 1) "Parameter name"
			param.name = reader.GetString()
			
			-- Read parameter data
			Verify (reader.nextMini() == 2) "Parameter value"
			if type == 0x10102 then (
				param.type  = PT_INT
				param.value = reader.GetLong()
			) else if type == 0x10103 then (
				param.type  = PT_FLOAT
				param.value = reader.GetFloat()
			) else if type == 0x10104 then (
				param.type = PT_FLOAT3
				param.value = [reader.GetFloat(), reader.GetFloat(), reader.GetFloat()]
			) else if type == 0x10105 then (
				param.type  = PT_TEXTURE
				param.value = reader.GetString()
			) else if type == 0x10106 then (
				param.type = PT_FLOAT4
				param.value = [reader.GetFloat(), reader.GetFloat(), reader.GetFloat(), reader.GetFloat()]
			)
			append params param
			type = reader.Next()
		)
		
		return params
	)
	
	fn ReadVertex_1 reader =
	(
		local vertex = VertexInfo()
		vertex.pos    = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat())
		reader.Seek(3 * 4) -- Skip normal
		vertex.tex      = Point3 (reader.GetFloat()) (1 - reader.GetFloat()) 0
		reader.Seek(12 * 4) -- Skip 3 UV pairs, tangent and binormal
		vertex.color    = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat())
		vertex.alpha    = reader.GetFloat()
		vertex.bones    = #(reader.GetLong() + 1, reader.GetLong() + 1, reader.GetLong() + 1, reader.GetLong() + 1)
		vertex.weights  = [reader.GetFloat(), reader.GetFloat(), reader.GetFloat(), reader.GetFloat()]
		return vertex;
	)

	fn ReadVertex_2 reader =
	(
		local vertex = VertexInfo()
		vertex.pos    = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat())
		reader.Seek(3 * 4) -- Skip normal
		vertex.tex      = Point3 (reader.GetFloat()) (1 - reader.GetFloat()) 0
		reader.Seek(12 * 4) -- Skip 3 UV pairs, tangent and binormal
		vertex.color    = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat())
		vertex.alpha    = reader.GetFloat()
		reader.Seek(4 * 4) -- Skip unused data
		vertex.bones    = #(reader.GetLong() + 1, reader.GetLong() + 1, reader.GetLong() + 1, reader.GetLong() + 1)
		vertex.weights  = [reader.GetFloat(), reader.GetFloat(), reader.GetFloat(), reader.GetFloat()]
		return vertex;
	)
	
	fn ReadMaterial reader isCollisionMesh =
	(
		local material = MaterialInfo()
		Verify (reader.Next() == 0x10100) "Material root"

		-- Read shader name
		Verify (reader.Next() == 0x10101) "Material name"
		material.name = reader.GetString()
		
		-- Read shader parameters
		material.params = ReadParameters reader
	
		-- Read geometry
		Verify (reader.Next() == 0x10000) "Material data"

		Verify (reader.Next() == 0x10001) "Material geometry size"
		material.vertices.count = reader.GetLong()
		material.faces.count    = reader.GetLong()

		Verify (reader.Next() == 0x10002) "Material vertex format" -- Skip vertex format

		-- Read vertices
		local type = reader.Next()
		Verify (type == 0x10007 or type == 0x10005) "Material vertex data"
		if type == 0x10005 then
		(
			-- Vertex format, revision 1
			for i = 1 to material.vertices.count do
				material.vertices[i] = ReadVertex_1 reader
		)
		else
		(
			-- Vertex format, revision 2
			for i = 1 to material.vertices.count do
				material.vertices[i] = ReadVertex_2 reader
		)
		
		-- Clear lookup map
		material.map.count = material.vertices.count
		for i = 1 to material.map.count do
		(
			material.map[i] = #()
		)

		-- Read faces
		Verify (reader.Next() == 0x10004) "Material face data"
		for i = 1 to material.faces.count do
		(
			-- +1 because Max arrays are one-based
			face = Point3 (reader.GetShort() + 1) (reader.GetShort() + 1) (reader.GetShort() + 1)
			material.faces[i] = face
			
			-- Add to lookup
			append material.map[face.x] i
			append material.map[face.y] i
			append material.map[face.z] i
		)
			
		type = reader.Next()
		if type == 0x10006 then 
		(
			-- Read bone mapping
			material.boneMapping.count = reader.Size() / 4;
			for i = 1 to material.boneMapping.count do
			(
				material.boneMapping[i] = reader.GetLong()
			)
			type = reader.Next()
		)
		
		if isCollisionMesh then
		(
			-- Skip collision mesh info
			Verify (type == 0x1200) "Material collision tree"
			reader.Skip()
			type = reader.Next()
		)
		
		Verify (type == -1) "End of material"
		return material
	)
			
	fn ReadMesh reader =
	(
		local mesh = MeshInfo()
		
		-- Read mesh name
		Verify (reader.Next() == 0x401) "Mesh name"
		mesh.name = reader.GetString()
		
		-- Read mesh information
		Verify (reader.Next() == 0x402) "Mesh data"
		mesh.materials.count = reader.GetLong()
		reader.Seek(7 * 4) -- Ignore bounding box information and unused long
		mesh.isHidden        = reader.GetLong() != 0
		mesh.isCollisionMesh = reader.GetLong() != 0
		
		-- Read materials
		for i = 1 to mesh.materials.count do
		(
			mesh.materials[i] = ReadMaterial reader mesh.isCollisionMesh
		)
		
		Verify (reader.Next() == -1) "End of mesh"
		return mesh
	)
	
	fn ReadLight reader =
	(
		local light = LightInfo()

		-- Read light name
		Verify (reader.Next() == 0x1301) "Light name"
		light.name = reader.GetString()

		-- Read light data
		Verify (reader.Next() == 0x1302) "Light data"
		light.type                = reader.GetLong()
		light.color               = Color (255 * reader.GetFloat()) (255 * reader.GetFloat()) (255 * reader.GetFloat())
		light.intensity           = reader.GetFloat()
		light.farAttenuationEnd   = reader.GetFloat()
		light.farAttenuationStart = reader.GetFloat()
		light.hotspotSize         = reader.GetFloat()
		light.falloffSize         = reader.GetFloat()
		
		Verify (reader.Next() == -1) "End of light"
		return light;
	)

	fn ReadProxy reader =
	(
		local proxy = ProxyInfo()
		Verify (reader.Next() == 0x603) "Proxy"
		Verify (reader.NextMini() == 5) "Proxy name"; proxy.name = reader.GetString()
		Verify (reader.NextMini() == 6) "Proxy bone"; local bone = reader.GetLong() + 1
		if bone > 1 then
		(
			-- Only assign if it's not to the root bone
			proxy.bone = bone - 1
		)
		
		local type = reader.NextMini()
		if type == 7 then
		(
			proxy.isHidden = reader.GetLong() != 0
			type = reader.NextMini()
		)
		if type == 8 then
		(
			proxy.altDecreaseStayHidden = reader.GetLong() != 0
			type = reader.NextMini()
		)
		Verify (type == -1) "End of proxy"
		return proxy
	)
	
	fn ReadDazzle reader =
	(
		local dazzle = DazzleInfo()
		Verify (reader.Next() == 0x604) "Dazzle"
		Verify (reader.Next() == 0) "Dazzle data"
		Verify (reader.NextMini() ==  0) "Dazzle color";    dazzle.color     = Color (255 * reader.GetFloat()) (255 * reader.GetFloat()) (255 * reader.GetFloat())
		Verify (reader.NextMini() ==  1) "Dazzle position"; dazzle.position  = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat())
		Verify (reader.NextMini() ==  2) "Dazzle radius";   dazzle.radius    = reader.GetFloat()
		Verify (reader.NextMini() ==  3) "Dazzle slot X";   dazzle.slotX     = reader.GetLong()
		Verify (reader.NextMini() ==  4) "Dazzle slot Y";   dazzle.slotY     = reader.GetLong()
		Verify (reader.NextMini() ==  6) "Dazzle #slots";   dazzle.numSlots  = reader.GetLong()
		Verify (reader.NextMini() ==  5) "Dazzle texture";  dazzle.texture   = reader.GetString()
		Verify (reader.NextMini() ==  7) "Dazzle freq";     dazzle.frequency = reader.GetFloat()
		Verify (reader.NextMini() ==  8) "Dazzle phase";    dazzle.phase     = reader.GetFloat()
		Verify (reader.NextMini() ==  9) "Dazzle night";    dazzle.nightOnly = reader.GetLong() != 0
		Verify (reader.NextMini() == 10) "Dazzle bone";     local bone       = reader.GetLong() + 1
		Verify (reader.NextMini() == 11) "Dazzle name";     dazzle.name      = reader.GetString()
		Verify (reader.NextMini() == 12) "Dazzle hidden";   dazzle.isHidden  = reader.GetLong() != 0

		if bone > 1 then
		(
			-- Only assign if it's not to the root bone
			dazzle.bone = bone - 1
		)
		
		local type = reader.NextMini()
		if type == 13 then
		(
			dazzle.bias = reader.GetFloat()
			type = reader.NextMini()
		)
		Verify (type == -1) "End of dazzle data"
		Verify (reader.Next() == -1) "End of dazzle"
		return dazzle
	)

	fn ReadModel reader =
	(
		local model = ModelInfo()
		
		-- Read bones
		Verify (reader.Next() == 0x200) "Skeleton"
		model.bones = ReadBones reader
		deleteItem model.bones 1	-- Remove the root bone
				
		-- Read objects
		local objects = #()
		local type = reader.Next()
		while type == 0x400 or type == 0x1300 do
		(
			if type == 0x400 then
			(
				local mesh = ReadMesh reader
				append model.meshes mesh
				append objects mesh
			)
			else
			(
				local light = ReadLight reader
				append model.lights light
				append objects light
			)
			type = reader.Next()
		)

		-- Read connections and proxies
		Verify(type == 0x600) "Connections"
		Verify (reader.Next() == 0x601) "Connection counts"
		Verify (reader.NextMini() == 1) "Connection objects"; local nConnections  = reader.GetLong()
		Verify (reader.NextMini() == 4) "Connection proxies"; model.proxies.count = reader.GetLong()
		
		type = reader.NextMini()
		if type == 9 then
		(
			model.dazzles.count = reader.GetLong()
			type = reader.NextMini()
		)
				
		for i = 1 to nConnections do
		(
			Verify (reader.Next() == 0x602) "Connection Object"
			Verify (reader.NextMini() == 2) "Connection Object index"; local idx  = reader.GetLong() + 1 	-- Max arrays are 1-based
			Verify (reader.NextMini() == 3) "Connection Object bone";  local bone = reader.GetLong() + 1
			if bone > 1 then
			(
				-- Only assign if it's not to the root bone
				objects[idx].bone = bone - 1
			)
		)
		
		for i = 1 to model.proxies.count do
		(
			local proxy = ReadProxy reader
			model.proxies[i] = proxy
		)

		for i = 1 to model.dazzles.count do
		(
			local dazzle = ReadDazzle reader
			model.dazzles[i] = dazzle
		)
		
		Verify (reader.Next() == -1) "End of connections"
		return model
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- Animation reader
	
	struct AnimFrameInfo(translation = Point3 0 0 0, rotation = Quat 0 0 0 1, scale = Point3 1 1 1, visibility = 1)
	struct AnimBoneInfo(name, bone, frames=#(), hasVisibilityTrack = false)
	struct UnpackInfo(transOffset, transScale, scaleOffset, scaleScale, idxTrans, idxScale, idxRotation)
	struct AnimationInfo(name, nFrames, fps, bones=#())

	fn UnpackQuaternion q1 q2 q3 q4 =
	(
		return Quat (q1 / 32767.0) (q2 / 32767.0) (q3 / 32767.0) (q4 / 32767.0)
	)
	
	-- Read a packed quaternion
	fn ReadPackedQuaternion reader =
	(
		return Quat (reader.GetSShort() / 32767.0) (reader.GetSShort() / 32767.0) (reader.GetSShort() / 32767.0) (reader.GetSShort() / 32767.0)
	)
	
	fn UnpackVector s1 s2 s3 scale offset =
	(
		return (Point3 s1 s2 s3) * scale + offset
	)
	
	-- Read a packed vector
	fn ReadPackedVector reader scale offset =
	(
		return (Point3 (reader.GetShort()) (reader.GetShort()) (reader.GetShort())) * scale + offset
	)

	fn ReadAnimationBone reader nFrames isFOC &unpack =
	(
		local bone = AnimBoneInfo()
			
		Verify (reader.Next() == 0x1002) "Animation bone"
		Verify (reader.Next() == 0x1003) "Animation bone information"
		Verify (reader.NextMini() ==  4) "Animation bone name"; bone.name = reader.GetString()
		Verify (reader.NextMini() ==  5) "Animation bone bone"; bone.bone = reader.GetLong()
		local type = reader.NextMini()
		if type == 10 then
		(
			-- We don't care about this one
			type = reader.NextMini()
		)
		Verify (type              ==  6) "Animation bone translation offset"; unpack.transOffset = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat()) 
		Verify (reader.NextMini() ==  7) "Animation bone translation scale";  unpack.transScale  = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat()) 
		Verify (reader.NextMini() ==  8) "Animation bone scale offset"; unpack.scaleOffset = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat()) 
		Verify (reader.NextMini() ==  9) "Animation bone scale scale";  unpack.scaleScale  = Point3 (reader.GetFloat()) (reader.GetFloat()) (reader.GetFloat()) 
		local defRotation = Quat 0 0 0 1
		if isFOC then
		(
			Verify (reader.NextMini() == 14) "Animation bone translation index"; unpack.idxTrans    = reader.GetSShort()
			Verify (reader.NextMini() == 15) "Animation bone scale index";       unpack.idxScale    = reader.GetSShort()
			Verify (reader.NextMini() == 16) "Animation bone rotation index";    unpack.idxRotation = reader.GetSShort()
			Verify (reader.NextMini() == 17) "Animation bone default rotation";  defRotation = ReadPackedQuaternion reader
		)

		-- Initialize default bone frames		
		bone.frames.count = nFrames
		for f = 1 to nFrames do
			bone.frames[f] = AnimFrameInfo translation:unpack.transOffset scale:unpack.scaleOffset rotation:defRotation
			
		type = reader.Next()
		if not isFOC then
		(
			-- Read per-bone track data
			if type == 0x1004 then
			(
				-- Read the translation data
				for f = 1 to nFrames do
					bone.frames[f].translation = ReadPackedVector reader unpack.transScale unpack.transOffset 
				type = reader.Next()
			)

			if type == 0x1005 then
			(
				-- Read the scale data
				for f = 1 to nFrames do
					bone.frames[f].scale = ReadPackedVector reader unpack.scaleScale unpack.scaleOffset 
				type = reader.Next()
			)

			Verify (type == 0x1006) "Animation bone default rotation EaW"
			if reader.Size() == 8 then
			(
				-- Reset default rotation
				local defRotation = ReadPackedQuaternion reader
				for f = 1 to nFrames do
					bone.frames[f].rotation = defRotation
			)
			else
			(
				-- Read the rotation data
				for f = 1 to nFrames do
					bone.frames[f].rotation = ReadPackedQuaternion reader
			)
			type = reader.Next()
		)
		
		if type == 0x1007 then
		(
			-- Read visibility track
			local val = 0
			for f = 1 to nFrames do
			(
				if mod f 8 == 1 then val = reader.GetByte()
				if (bit.and val 1) == 0 then
				(
					bone.frames[f].visibility = 0
					bone.hasVisibilityTrack   = true
				)
				val = bit.shift val -1 
			)
			type = reader.Next()
		)
		
		if type == 0x1008 then
		(
			type = reader.Next()
		)

		Verify (type == -1) "Animation bone end"
		return bone
	)
	
	fn ReadAnimation name model reader =
	(
		local animation = AnimationInfo name:name
		Verify (reader.Next() == 0x1000) "Animation root"
		Verify (reader.Next() == 0x1001) "Animation info"
		Verify (reader.NextMini() == 1) "Animation info #frames"; animation.nFrames     = reader.GetLong()
		Verify (reader.NextMini() == 2) "Animation info FPS";     animation.fps         = reader.GetFloat()
		Verify (reader.NextMini() == 3) "Animation info #bones";  animation.bones.count = reader.GetLong()
		local type = reader.NextMini()
		
		local nRotationWords, nTranslationWords, nScaleWords
		local isFOC = (type != -1)
		if isFOC then
		(
			Verify (type == 11) "Animation info #rotations";                 nRotationWords    = reader.GetLong()
			Verify (reader.NextMini() == 12) "Animation info #translations"; nTranslationWords = reader.GetLong()
			Verify (reader.NextMini() == 13) "Animation info #scales";       nScaleWords       = reader.GetLong()
		)
		
		-- Read bone data
		local unpacks = #()
		for i = 1 to animation.bones.count do
		(
			local unpack = UnpackInfo()
			animation.bones[i] = ReadAnimationBone reader animation.nFrames isFOC &unpack
			append unpacks unpack

			-- Verify bone
			Verify (animation.bones[i].bone <= model.bones.count) "Animation bone sanity #1"
			Verify (animation.bones[i].name == model.bones[animation.bones[i].bone].name) "Animation bone sanity #2"
		)
		
		if isFOC then
		(
			if nTranslationWords > 0 then
			(
				-- Read translation data
				Verify (reader.Next() == 0x100a) "Animation translation words"
				Verify (reader.Size() == animation.nFrames * nTranslationWords * 2) "Animtion translation words size"
				
				local data = #()
				data.count = animation.nFrames * nTranslationWords
				for i = 1 to animation.nFrames * nTranslationWords do
					data[i] = reader.GetShort()
					
				for i = 1 to animation.bones.count do
				(
					if unpacks[i].idxTrans != -1 then
					(
						for f = 1 to animation.nFrames do
						(
							local index = (f - 1) * nTranslationWords + unpacks[i].idxTrans + 1
							animation.bones[i].frames[f].translation = UnpackVector data[index] data[index+1] data[index+2] unpacks[i].transScale unpacks[i].transOffset
						)
					)
				)
			)
			
			if nRotationWords > 0 then
			(
				-- Read rotation data
				Verify (reader.Next() == 0x1009) "Animation rotation words"
				Verify (reader.Size() == animation.nFrames * nRotationWords * 2) "Animation rotation words size"
				
				local data = #()
				data.count = animation.nFrames * nRotationWords
				for i = 1 to animation.nFrames * nRotationWords do
					data[i] = reader.GetSShort()
					
				for i = 1 to animation.bones.count do
				(
					if unpacks[i].idxRotation != -1 then
					(
						for f = 1 to animation.nFrames do
						(
							local index = (f - 1) * nRotationWords + unpacks[i].idxRotation + 1
							animation.bones[i].frames[f].rotation = UnpackQuaternion data[index] data[index+1] data[index+2] data[index+3]
						)
					)
				)
			)
		)

		animation.nFrames = animation.nFrames - 1

		Verify (reader.Next() == -1) "Animation end"
		return animation
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- Mesh creation, etc.
	
	local MAX_COINCIDENT_DIST = 0.001 -- Weld vertices closer than this together
	
	fn LoadTexture filename = 
	(
		local bitmap
		for dir in textures.items while bitmap == undefined do
		(
			if dir[dir.count] != "\\" then dir = dir + "\\"
			try
			(
				-- First try the filename as provided
				bitmap = openBitMap(dir + filename)
			)
			catch
			(
				-- Then try with dds extension
				try
				(
					filename = (GetFilenamePath filename) + (GetFilenameFile filename) + ".dds"
					bitmap = openBitMap(dir + filename)
				)
				catch
				(
					bitmap = undefined
				)
			)
		)
		if bitmap == undefined then
			throw ("Unable to load texture: " + filename)
		return bitmap
	)

	fn LoadShader filename =
	(
		local shader
		for dir in shaders.items while shader == undefined do
		(
			if dir[dir.count] != "\\" then dir = dir + "\\"
			try
			(
				shader = DirectX_9_Shader effectFile:(dir + filename)

				-- Check if we loaded the correct shader
				-- (default shader is loaded when specified one isn't found)
				if shader.effectFilename != (dir + filename) then
					shader = undefined
			)
			catch
			(
				shader = undefined
			)
		)
		if shader == undefined then
			print ("Warning: unable to load shader: " + filename)
		return shader
	)

	-- Relative simple hash function
	fn HashVector v =
	(
		return abs(v.x) + abs(v.y) + abs(v.z);
	)
	
	-- Are two vectors at the same position?
	fn EqualVector v1 v2 =
	(
		return
			abs(v1.x - v2.x) < MAX_COINCIDENT_DIST and
			abs(v1.y - v2.y) < MAX_COINCIDENT_DIST and
			abs(v1.z - v2.z) < MAX_COINCIDENT_DIST
	)

	-- Are two matrices equal the same position?
	fn EqualMatrix m1 m2 =
	(
		return
			(EqualVector m1.row1 m2.row1) and
			(EqualVector m1.row2 m2.row2) and
			(EqualVector m1.row3 m2.row3) and
			(EqualVector m1.row4 m2.row4)
	)

	struct UniqueSet(verts = #(), faces = #(), mats = #(), map = #(), weights = #(), bones = #())
	
	fn CreateUniqueSet mesh prop isGeometry =
	(
		local out  = UniqueSet()
		local dict = Dictionary()
		local maps = #()
		
		-- Build the uniqe vertex array
		maps.count = mesh.materials.count
		for m = 1 to mesh.materials.count do
		(
			local mat = mesh.materials[m]
			local map = #()
			map.count = mat.vertices.count
			for i = 1 to mat.vertices.count do
			(
				local v = getProperty mat.vertices[i] prop

				-- Find the vertex
				local hash  = HashVector v
				local verts = dict.Get hash
				local pos   = -1
				if verts != undefined then
				(
					for j = 1 to verts.count while pos == -1 do
					(
						if EqualVector out.verts[verts[j]] v then
						(
							pos = verts[j]
						)
					)
				)
				else verts = #()
				
				if pos == -1 then
				(
					-- New vector, add it
					append out.verts v
					if isGeometry and mat.boneMapping.count > 0 then
					(
						local bone = mat.boneMapping[mat.vertices[i].bones[1]]
						if bone > 0 then
						(
							append out.weights mat.vertices[i].weights[1]
							append out.bones   bone
						)
					)
					pos = out.verts.count
					append verts pos
					dict.Put hash verts
				)
				
				-- Map it
				map[i] = pos
			)
			maps[m] = map
		)
		
		if isGeometry then
		(
			-- Clear the lookup map and weight table
			out.map.count     = out.verts.count
			for i = 1 to out.map.count do out.map[i] = #()
		)
		
		-- Create a matching faces array
		for m = 1 to mesh.materials.count do
		(
			local mat = mesh.materials[m]
			local map = maps[m]

			-- Increase array sizes			
			local base = out.faces.count
			out.faces.count = out.faces.count + mat.faces.count
			if isGeometry then
			(
				out.mats.count = out.mats.count + mat.faces.count
			)
			
			-- Calculate faces
			for f = 1 to mat.faces.count do
			(
				face = mat.faces[f]
				out.faces[base + f] = Point3 map[face.x] map[face.y] map[face.z]
				if isGeometry then
				(
					out.mats[base + f] = m
					append out.map[map[face.x]] (base + f)
					append out.map[map[face.y]] (base + f)
					append out.map[map[face.z]] (base + f)
				)
			)
		)
		
		return out;
	)
	
	fn CreateMaterial name params =
	(
		local shader = LoadShader name
		if shader != undefined then
		(
			for p in params do
			(
				if hasProperty shader p.name then
				(
					local value = p.value
					if p.type == PT_TEXTURE then
					(
						-- We've got to load the texture first
						value = LoadTexture p.value
					)
					setProperty shader p.name value
				)
			)
		)
		return shader
	)
	
	struct FaceConnection(face, smooth = false)
	struct FaceInfo(connections = #(), group = -1)
	struct FaceGroup(faces = #(), connections = #(), color = -1)
	
	--
	-- Calculate and set smoothing groups.
	-- Faces that share a vertex (i.e, have no split vertices) are part of the
	-- same smoothing group. This way the edge between them does not appear hard.
	--
	fn SetSmoothingGroups maxmesh srcmesh verts = 
	(
		local faces = #()
		faces.count = verts.faces.count
		for i = 1 to faces.count do faces[i] = FaceInfo()
		
		-- Determine face connections
		for f in verts.map do
		(
			for i = 1 to f.count do
			for j = i + 1 to f.count do
			(
				-- f[i] and f[j] share a vertex and are thus connected
				append faces[f[i]].connections (FaceConnection face:f[j])
				append faces[f[j]].connections (FaceConnection face:f[i])
			)
		)

		-- Determine smoothed connections
		local base = 0
		for mat in srcmesh.materials do
		(
			for f in mat.map do
			(
				for i = 1 to f.count do
				for j = i + 1 to f.count do
				(
					-- f[i] and f[j] share a vertex and are thus smoothed
					local f1 = base + f[i];
					local f2 = base + f[j];
					local done = false;
					for k = 1 to faces[f1].connections.count while not done do
						if faces[f1].connections[k].face == f2 then
						(
							faces[f1].connections[k].smooth = true
							done = true
						)
						
					done = false
					for k = 1 to faces[f2].connections.count while not done do
						if faces[f2].connections[k].face == f1 then
						(
							faces[f2].connections[k].smooth = true
							done = true
						)
				)
			)
			base += mat.faces.count
		)
		
		-- Create groups
		local groups = #()
		for i = 1 to faces.count do
		(
			if faces[i].group < 1 then
			(
				-- Create a new group
				append groups (FaceGroup())
				faces[i].group = groups.count
			)

			-- Add us to our group
			append groups[faces[i].group].faces i
					
			for f in faces[i].connections do
			(
				if not f.smooth then
				(
					-- Carry the connection over to the group
					append groups[faces[i].group].connections f.face
				)
				else
				(
					-- This face is part of the same group
					faces[f.face].group = faces[i].group
				)
			)
		)

		-- Simplify group connections
		for i = 1 to groups.count do
		(
			local group = groups[i]
			connections = #()
			for f in group.connections do 
			(
				append connections faces[f].group
			)
			group.connections = connections
		)
		
		-- Calculate colors
		for i = 1 to groups.count do
		(
			group = groups[i]
			local available = #()
			for i = 1 to 32 do available[i] = true
			
			for g in group.connections do
			(
				if groups[g].color > 0 then
				(
					available[groups[g].color] = false
				)
			)
			group.color = findItem available true
		)

		-- Assign smoothing groups
		for i = 1 to groups.count do
		(
			group = groups[i]
			for j = 1 to group.faces.count do
			(
				if group.color > 0 then color = 2^(group.color - 1) else color = 0
				setFaceSmoothGroup maxmesh group.faces[j] color
			)
		)
	)
	
	fn CreateMesh model srcmesh =
	(
		local posVerts = CreateUniqueSet srcmesh "pos" true
		local tvVerts  = CreateUniqueSet srcmesh "tex" false
		local vcVerts  = CreateUniqueSet srcmesh "color" false

		-- Create the mesh
		local maxmesh = Mesh name:srcmesh.name vertices:posVerts.verts faces:posVerts.faces materialIDs:posVerts.mats tverts:tvVerts.verts
		
		SetSmoothingGroups maxmesh srcmesh posVerts

		-- Create texture faces
		buildTVFaces maxmesh
		for i = 1 to tvVerts.faces.count do	setTVFace maxmesh i tvVerts.faces[i]

		-- Create color faces
		setNumCPVVerts maxmesh vcVerts.verts.count
		buildVCFaces maxmesh
		for i = 1 to vcVerts.verts.count do setVertColor maxmesh i vcVerts.verts[i]
		for i = 1 to vcVerts.faces.count do setVCFace    maxmesh i vcVerts.faces[i]

		-- Create material
		if srcmesh.materials.count == 1 then
		(
			-- Single material
			maxmesh.material = CreateMaterial srcmesh.materials[1].name srcmesh.materials[1].params
		)
		else if srcmesh.materials.count > 0 then
		(
			-- Multi-material
			maxmesh.material = MultiMaterial numsubs:srcmesh.materials.count
			for i = 1 to srcmesh.materials.count do
				maxmesh.material[i] = CreateMaterial srcmesh.materials[i].name srcmesh.materials[i].params
		)

		SetUserProp maxmesh "Alamo_Export_Transform" true
		SetUserProp maxmesh "Alamo_Export_Geometry"  true
		SetUserProp maxmesh "Alamo_Collision_Enabled" srcmesh.isCollisionMesh
		SetUserProp maxmesh "Alamo_Geometry_Hidden"   srcmesh.isHidden
		SetUserProp maxmesh "Alamo_Alt_Decrease_Stay_Hidden" false
		if srcmesh.bone != undefined then
		(
			SetUserProp maxmesh "Alamo_Billboard_Mode"  model.bones[srcmesh.bone].billboardMode
		)
		maxmesh.transform = Matrix3 1
		srcmesh.vweights  = posVerts.weights 
		srcmesh.vbones    = posVerts.bones
		
		return maxmesh;
	)
	
	fn CreateLight model light =
	(
		local maxlight
		case light.type of
		(
			LT_SPOTLIGHT:	maxlight = FreeSpot name:light.name falloff:(radTodeg light.falloffSize) hotspot:(radToDeg light.hotspotSize)
			PT_DIRECTIONAL:	maxlight = DirectionalLight name:light.name
			PT_OMNI:		maxlight = OmniLight name:light.name
		)
		maxlight.rgb           = light.color
		maxlight.multiplier    = light.intensity
		maxlight.farAttenStart = light.farAttenuationStart
		maxlight.farAttenEnd   = light.farAttenuationEnd
		maxlight.transform     = Matrix3 1
		return maxlight
	)
	
	fn CreateProxy model proxy =
	(
		local maxproxy = Alamo_Proxy name:proxy.name
		SetUserProp maxproxy "Alamo_Geometry_Hidden"          proxy.isHidden
		SetUserProp maxproxy "Alamo_Alt_Decrease_Stay_Hidden" proxy.altDecreaseStayHidden
		maxproxy.transform = Matrix3 1
		return maxproxy
	)
	
	fn CreateDazzle model dazzle =
	(
		local maxdazzle = Alamo_Dazzle name:dazzle.name
		AlamoImporter.Helper.InitializeDazzle maxdazzle dazzle.radius dazzle.color dazzle.slotX dazzle.slotY dazzle.numSlots dazzle.texture dazzle.frequency dazzle.phase dazzle.bias dazzle.nightOnly
		
		SetUserProp maxdazzle "Alamo_Geometry_Hidden" dazzle.isHidden
		maxdazzle.transform = transMatrix dazzle.position
		return maxdazzle
	)

	struct ObjectsInfo(bones = #(), meshes = #(), lights = #(), proxies = #(), dazzles = #())
	struct BoneChildren(objects = #(), skinned = false)
	
	fn CreateSkeleton model objects root =
	(
		-- Construct a list of children for each bone, and root objects
		local children = #()
		local roots    = #()
		children.count = model.bones.count
		for i = 1 to model.bones.count do children[i] = BoneChildren()
		for i = 1 to objects.meshes.count  do if model.meshes[i].bone  != undefined then append children[model.meshes[i].bone].objects  objects.meshes[i]  else append roots objects.meshes[i]
		for i = 1 to objects.lights.count  do if model.lights[i].bone  != undefined then append children[model.lights[i].bone].objects  objects.lights[i]  else append roots objects.lights[i]
		for i = 1 to objects.proxies.count do if model.proxies[i].bone != undefined then append children[model.proxies[i].bone].objects objects.proxies[i] else append roots objects.proxies[i]
		for i = 1 to objects.dazzles.count do if model.dazzles[i].bone != undefined then append children[model.dazzles[i].bone].objects objects.dazzles[i] else append roots objects.dazzles[i]
		
		if root != undefined then
		(
			-- Attach root objects to root
			for obj in roots do
			(
				attachObjects root obj move:false
				obj.transform = obj.transform * root.transform
			)
		)
		
		-- Mark the bones that take part in skinned animation
		for i = 1 to model.meshes.count do
		for j = 1 to model.meshes[i].materials.count do
		(
			local mat = model.meshes[i].materials[j]
			for b in mat.boneMapping do
				if b > 0 then
					children[b].skinned = true
		)
		
		-- We can replace a bone by it's child object if it has only one child object
		-- and it does not take part in skinning
		local bones      = #()
		local transforms = #()
		bones.count      = model.bones.count
		transforms.count = model.bones.count
		rootTransform    = Matrix3 1
		if root != undefined then rootTransform = root.transform
		
		for i = 1 to children.count do
		(
			-- Get bone transform
			local transform = model.bones[i].transform
			if model.bones[i].parent != undefined then (
				transform = transform * transforms[model.bones[i].parent]
			) else (
				transform = transform * rootTransform
			)
			transforms[i] = transform

			if children[i].objects.count == 1 and not children[i].skinned then (
				-- Use the object as bone
				bones[i] = children[i].objects[1]
				bones[i].transform = bones[i].transform * transform
			) else (
				-- Create the bone
				bones[i] = boneSys.createBone transform.translationPart (transform.translationPart + transform.row1) transform.row3
				bones[i].name   = model.bones[i].name
				bones[i].width  = 1
				bones[i].height = 1
				bones[i].boneScaleType = #none -- Avoid that the bone rescales during animations
				bones[i].transform = transform
				
				-- Attach its children objects
				for j = 1 to children[i].objects.count do
				(
					attachObjects bones[i] children[i].objects[j] move:false
					children[i].objects[j].transform = children[i].objects[j].transform * transform
				)
			)

			if model.bones[i].parent != undefined then
			(
				-- Attach bone to parent
				attachObjects bones[model.bones[i].parent] bones[i] move:false
			)
			else if root != undefined then
			(
				attachObjects root bones[i] move:false
			)
		)
		
		return bones
	)

	-- Apply skin modifier to skinned meshes
	fn SkinMeshes model objects =
	(	
		-- Determine if we need to skin
		for i = 1 to model.meshes.count do
		(
			local skinned = false
			for j = 1 to model.meshes[i].materials.count while not skinned do
			(
				if model.meshes[i].materials[j].boneMapping.count > 0 then
				(
					-- This material is skinned
					skinned = true
				)
			)
		
			if skinned then
			(
				-- Add skin modifier
				max modify mode
				select objects.meshes[i]

				local theskin = Skin()
				addModifier objects.meshes[i] theskin
				theskin.weightAllVertices = false

				-- Add used bones
				local vbones   = model.meshes[i].vbones
				local vweights = model.meshes[i].vweights
				local bones    = #()
				bones.count = model.bones.count
				for b in vbones do
				(
					bones[b] = b
				)

				local numBones = 0	
				for b in bones do
				(
					if b != undefined then
					(
						skinOps.addBone theskin objects.bones[b] 0

						numBones = numBones + 1;
						bones[b] = numBones
						
						local no = skinOps.getNumberCrossSections theskin numBones
						for k = 1 to no do
						(
							skinOps.SetInnerRadius theskin numBones k 0
							skinOps.SetOuterRadius theskin numBones k 0
						)
					)
				)
				
				-- This updates the internal cache; it's required
				GetNumVerts objects.meshes[i]
			
				-- Set vertex weights
				for j = 1 to vbones.count do
				(
					skinOps.setVertexWeights theskin j bones[vbones[j]] vweights[j]
				)
				
				update objects.meshes[i]
			)
		)		
	)
	
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	------------------------------------------------------------------------------------
	-- Import entry function
	
	on doImport pressed do
	(	
		if textures.items.count == 0 then
			messageBox "Please add one or more texture paths before importing"
		else if shaders.items.count == 0 then
			messageBox "Please add one or more shaders paths before importing"
		else try
		(
			local filename = getOpenFileName caption:"Import Alo" types:"Alamo Object (*.alo)|*.alo|All Files (*.*)|*.*|"
			if filename != undefined then in coordsys world
			with redraw off
			with undo off
			(
				-- Get a list of animation files
				local basename = (GetFilenameFile filename)
				local animationFiles = GetFiles ((GetFilenamePath filename) + basename + "_*.ALA")

				AlamoImporter.Helper.OpenProgressDialog()

				-- Read the model file
				AlamoImporter.Helper.SetProgress "Reading model..." 0.0
				local reader = ChunkReader()
				reader.Open filename
				local model = ReadModel reader
				reader.Close
				
				attachmentBone = undefined
				if objects.count > 0 then
				(
					CreateDialog selectNode modal:true width:300
				)
				
				-- Read the animation files
				local animations = #()
				for i = 1 to animationFiles.count do
				(
					local filename = (GetFilenameFile animationFiles[i])
					local name     = (substring filename (basename.count + 2) (filename.count - basename.count - 1))

					AlamoImporter.Helper.SetProgress ("Reading animation " + name + "...") (0.10 * i / animationFiles.count)

					try
					(
						local reader = ChunkReader()
						reader.Open animationFiles[i]
						local animation = (ReadAnimation name model reader)
						reader.Close
						append animations animation
					)
					catch
					(
						-- Skip this animation
						print ("Skipping animation " + filename)
					)
				)

				local fps
				if animations.count > 0 then
				(
					-- All animations must have the same framerate
					fps = animations[1].fps
					for animation in animations do
					(
						if animation.fps != fps then
							throw "All animations must have the same frame rate"
					)
				)
							
				-- Count number of objects to create
				local nObjects = model.meshes.count + model.lights.count + model.proxies.count + model.dazzles.count
				local iObject  = 0
				
				max create mode

				-- Create the meshes, lights and proxies
				local objs = ObjectsInfo()
				for mesh in model.meshes do
				(
					AlamoImporter.Helper.SetProgress "Creating objects..." (0.10 + 0.65 * iObject / nObjects)
					append objs.meshes (CreateMesh model mesh)
					iObject = iObject + 1
				)
				
				for light in model.lights do
				(
					AlamoImporter.Helper.SetProgress "Creating objects..." (0.10 + 0.65 * iObject / nObjects)
					append objs.lights  (CreateLight model light)
					iObject = iObject + 1
				)
				
				for proxy in model.proxies do
				(
					AlamoImporter.Helper.SetProgress "Creating objects..." (0.10 + 0.65 * iObject / nObjects)
					append objs.proxies (CreateProxy model proxy)
					iObject = iObject + 1
				)

				for dazzle in model.dazzles do
				(
					AlamoImporter.Helper.SetProgress "Creating objects..." (0.10 + 0.65 * iObject / nObjects)
					append objs.dazzles (CreateDazzle model dazzle)
					iObject = iObject + 1
				)

				-- Create the skeleton
				AlamoImporter.Helper.SetProgress "Creating skeleton..." 0.75
				objs.bones = CreateSkeleton model objs attachmentBone
				
				-- Skin the meshes
				AlamoImporter.Helper.SetProgress "Skinning model..." 0.78
				SkinMeshes model objs

				-- Apply the animations
				if animations.count > 0 then with animate on
				(
					local base = 1
					frameRate  = fps
					AlamoImporter.Helper.ClearAnimations()

					-- Determine which bones need a visibility track
					local hasVisibilityTrack = #()
					for animation in animations do
					(
						for bone in animation.bones do
						(
							if bone.hasVisibilityTrack then
								hasVisibilityTrack[bone.bone] = true
						)
					)

					-- Create the visibility tracks
					for i = 1 to model.bones.count do
					(
						if hasVisibilityTrack[i] == true then
							objs.bones[i].visibility = bezier_float()
					)

					for i = 1 to animations.count do
					(
						AlamoImporter.Helper.SetProgress "Applying animations..." (0.80 + 0.20 * (i - 1) / animations.count)
						local animation = animations[i]

						for bone in animation.bones do
						(
							local prev_visibility = undefined
							for f = 1 to animation.nFrames do at time (base + f - 1)
							(
								local transform = (inverse bone.frames[f].rotation) as Matrix3
								translate transform bone.frames[f].translation
								if model.bones[bone.bone].parent != undefined then
								(
									transform = transform * objs.bones[model.bones[bone.bone].parent].transform
								)
								objs.bones[bone.bone].transform = transform

								if hasVisibilityTrack[bone.bone] == true and prev_visibility != bone.frames[f].visibility then
								(
									local controller = objs.bones[bone.bone].visibility.controller

									k = AddNewKey controller (base + f - 1)
									k.value          = bone.frames[f].visibility
									k.inTangentType  = #step
									k.outTangentType = #step
									prev_visibility  = bone.frames[f].visibility
								)
							)
						)
						AlamoImporter.Helper.AddAnimation animation.name i base (base + animation.nFrames - 1)
						base += animation.nFrames
					)
					animationRange = interval 0 base
				)
				AlamoImporter.Helper.SetProgress "Done!" 1.0
				AlamoImporter.Helper.CloseProgressDialog()
			)
		)
		catch
		(
			AlamoImporter.Helper.CloseProgressDialog()
			messageBox (getCurrentException())
		)
	)
)
