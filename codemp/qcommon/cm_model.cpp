#include "cm_local.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

#include <vector>

static Assimp::Importer imp;

int CM_GetModelVerticies(char const * name, vec3_t * points, int points_num) {
	fileHandle_t file;
	long len = FS_FOpenFileRead(name, &file, qfalse);
	if (len < 1) {
		FS_FCloseFile(file);
		return -1;
	}
	char * buf = new char[len];
	FS_Read(buf, len, file);
	aiScene const * scene = imp.ReadFileFromMemory(buf, len, aiProcess_JoinIdenticalVertices);
	int p = 0;
	for (unsigned int m = 0; m < scene->mNumMeshes; m++) {
		if (p == points_num) break;
		aiMesh const * mesh = scene->mMeshes[m];
		for (unsigned int v = 0; v < mesh->mNumVertices; v++) {
			if (p == points_num) break;
			points[p][0] = mesh->mVertices[v].x;
			points[p][1] = mesh->mVertices[v].y;
			points[p][2] = mesh->mVertices[v].z;
			p++;
		}
	}
	FS_FCloseFile(file);
	return p;
}

static void CM_FreePObj(pObjSurface_t * surf) {
	if (surf->verts) delete[] surf->verts;
	if (surf->UVs) delete[] surf->UVs;
	if (surf->normals) delete[] surf->normals;
	if (surf->faces) delete[] surf->faces;
}

float testVerts[] = {
	0.0f, 0.0f, 0.0f,
	100.0f, 0.0f, 0.0f,
	100.0f, 100.0f, 0.0f
};

float testUVs[] = {
	0.0f, 0.0f,
	1.0f, 0.0f,
	1.0f, 1.0f,
};

#define OBJ_MAX_INDICIES 1024
#define CMD_BUF_LEN 12
#define FLOAT_BUF_LEN 15

typedef struct objIndex_s{
	int vi = -1;
	int uvi = -1;
	int ni = -1;
} objIndex_t;

char const * defaultShader = "textures/imperial/basic";

pObjSurface_t * CM_LoadPObj(char const * name) {
	fileHandle_t file;
	long len = FS_FOpenFileRead(name, &file, qfalse);
	if (len < 0) return nullptr;
	if (len == 0) {
		FS_FCloseFile(file);
		return nullptr;
	}
	char * buf = new char[len]();
	FS_Read(buf, len, file);
	FS_FCloseFile(file);
	pObjSurface_t * surf = new pObjSurface_t;
	float * verts = new float[OBJ_MAX_INDICIES * 3]();
	unsigned int verts_index = 0;
	surf->verts = verts;
	float * uvs = new float[OBJ_MAX_INDICIES * 2]();
	unsigned int uvs_index = 0;
	surf->UVs = uvs;
	float * normals = new float[OBJ_MAX_INDICIES * 3]();
	unsigned int normals_index = 0;
	surf->normals = normals;
	std::vector<objIndex_t> indicies;
	bool all_good = true;
	long i = 0;
	bool seekline = false;
	while (i < len && all_good) {
		switch (buf[i]) {
		case '\r':
			i++;
			continue;
		case '\n':
			i++;
			seekline = false;
			continue;
		case '\0':
			all_good = false;
			continue;
		case '#':
			i++;
			seekline = true;
			continue;
		default:
			if (seekline) {
				i++;
				continue;
			}
			break;
		}
		char cmd_buf[CMD_BUF_LEN];
		int ci;
		for (ci = 0; ci < CMD_BUF_LEN; ci++) {
			switch(buf[i+ci]) {
			case ' ':
				cmd_buf[ci] = '\0';
				break;
			default:
				cmd_buf[ci] = buf[i+ci];
				continue;
			}
			break;
		}
		if (ci == CMD_BUF_LEN) {
			all_good = false;
			break;
		} else {
			i += ci + 1;
		}
		if (!strcmp(cmd_buf, "v")) {
			char float_buf[FLOAT_BUF_LEN];
			for (int v = 0; v < 3; v++) {
				int vi;
				for(vi = 0; vi < FLOAT_BUF_LEN; vi++) {
					switch(buf[i + vi]) {
					case '\r':
					case '\n':
					case ' ':
						float_buf[vi] = '\0';
						break;
					default:
						float_buf[vi] = buf[i+vi];
						continue;
					}
					break;
				}
				i += vi + 1;
				float val = strtod(float_buf, nullptr);
				verts[verts_index] = val;
				verts_index++;
				memset(float_buf, FLOAT_BUF_LEN, sizeof(char));
			}
			i-=2;
			seekline = true;
			continue;
		} else if (!strcmp(cmd_buf, "vn")) {
				char float_buf[FLOAT_BUF_LEN];
				for (int v = 0; v < 3; v++) {
					int vi;
					for(vi = 0; vi < FLOAT_BUF_LEN; vi++) {
						switch(buf[i + vi]) {
						case '\r':
						case '\n':
						case ' ':
							float_buf[vi] = '\0';
							break;
						default:
							float_buf[vi] = buf[i+vi];
							continue;
						}
						break;
					}
					i += vi + 1;
					float val = strtod(float_buf, nullptr);
					normals[normals_index] = val;
					normals_index++;
					memset(float_buf, FLOAT_BUF_LEN, sizeof(char));
				}
				i-=2;
				seekline = true;
				continue;
		} else if (!strcmp(cmd_buf, "vt")) {
			char float_buf[FLOAT_BUF_LEN];
			for (int v = 0; v < 2; v++) {
				int vi;
				for(vi = 0; vi < FLOAT_BUF_LEN; vi++) {
					switch(buf[i + vi]) {
					case '\r':
					case '\n':
					case ' ':
						float_buf[vi] = '\0';
						break;
					default:
						float_buf[vi] = buf[i+vi];
						continue;
					}
					break;
				}
				i += vi + 1;
				float val = strtod(float_buf, nullptr);
				uvs[uvs_index] = val;
				uvs_index++;
				memset(float_buf, FLOAT_BUF_LEN, sizeof(char));
			}
			i-=2;
			seekline = true;
			continue;
		} else if (!strcmp(cmd_buf, "f")) {
			for (int fi = 0; fi < 3; fi++) {
				objIndex_t face;
				char int_buf[FLOAT_BUF_LEN];
				for (int v = 0; v < 3; v++) {
					int vi;
					for(vi = 0; vi < FLOAT_BUF_LEN; vi++) {
						switch(buf[i + vi]) {
						case '\r':
						case '\n':
						case ' ':
							int_buf[vi] = '\0';
							v += 3;
							break;
						case '\\':
						case '/':
							int_buf[vi] = '\0';
							break;
						default:
							int_buf[vi] = buf[i+vi];
							continue;
						}
						break;
					}
					i += vi + 1;
					int val = strtol(int_buf, nullptr, 10) - 1;
					switch(v) {
					case 0:
					case 3:
						face.vi = val;
					case 1:
					case 4:
						face.uvi = val;
					case 2:
					case 5:
						face.ni = val;
					}
					memset(int_buf, FLOAT_BUF_LEN, sizeof(char));
				}
				indicies.push_back(face);
			}
			i-=2;
			seekline = true;
			continue;
		} else {
			seekline = true;
			continue;
		}
	}
	delete[] buf;
	if (all_good) {
		Com_Printf("Obj Load Successful.");
		Com_Printf("Verts: %i\n", verts_index);
		for (size_t vs = 0; vs < verts_index; vs+=3) {
			Com_Printf("%i: (%f, %f, %f)\n", vs/3, verts[vs+0], verts[vs+1], verts[vs+2]);
		}
		Com_Printf("UVs: %i\n", uvs_index);
		for (size_t vs = 0; vs < uvs_index; vs+=2) {
			Com_Printf("%i: (%f, %f)\n", vs/2, uvs[vs+0], uvs[vs+1]);
		}
		Com_Printf("Normals: %i\n", normals_index);
		for (size_t vs = 0; vs < normals_index; vs+=3) {
			Com_Printf("%i: (%f, %f, %f)\n", vs/3, normals[vs+0], normals[vs+1], normals[vs+2]);
		}
		Com_Printf("Faces: %i\n", (int)indicies.size());
		int fi = 0;
		for (objIndex_t const & face : indicies) {
			Com_Printf("%i: (%i, %i, %i)\n", fi, face.vi, face.uvi, face.ni);
			fi++;
		}
	} else {
		Com_Printf("Obj Load Failed.\n");
		CM_FreePObj(surf);
		return nullptr;
	}

	surf->verts = verts;
	surf->numVerts = verts_index;
	surf->UVs = uvs;
	surf->numUVs = uvs_index;
	surf->normals = normals;
	surf->numNormals = normals_index;
	surf->numFaces = indicies.size() / 3;
	pObjFace_t * faces = new pObjFace_t[surf->numFaces];
	assert(surf->numFaces % 3 == 0);
	for (int f = 0; f < surf->numFaces; f++) {
		for (int fi = 0; fi < 3; fi++) {
			objIndex_t const & index = indicies.at((3 *f)+ (2 - fi));
			faces[f][fi].vertex = surf->verts + (index.vi * 3);
			faces[f][fi].uv = surf->UVs + (index.uvi * 2);
			faces[f][fi].normal = surf->normals + (index.ni * 3);
		}
	}
	surf->faces = faces;
	memcpy(surf->shader, defaultShader, strlen(defaultShader));
	return surf;
}
