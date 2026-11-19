/* ============================================================================
 * 09_buffers.inc -- part of render_c3d.cpp (SINGLE TRANSLATION UNIT).
 *
 * Vertex / index / normal buffer management (linearAlloc).
 *
 * Not a standalone header: no include guard and no includes of its own. It is
 * #included in a fixed order from render_c3d.cpp, which says why.
 * ========================================================================== */

/*===================================================================
	Vertex / Index buffer management
	Phase 2: use linearAlloc for GPU-visible memory
===================================================================*/

bool FSCreateVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->vertexBuffer = linearAlloc(numVertices * sizeof(LVERTEX));
	return renderObject->vertexBuffer != nullptr;
}

bool FSCreateDynamicVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	return FSCreateVertexBuffer(renderObject, numVertices);
}

bool FSCreateNormalBuffer(RENDEROBJECT *renderObject, int numNormals)
{
	renderObject->normalBuffer = linearAlloc(numNormals * sizeof(NORMAL));
	return renderObject->normalBuffer != nullptr;
}

bool FSCreateIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	/* numIndices is an index-element count at every call site (model and
	 * water loaders pass triangleCount*3, dynamic buffers pass 32767*3),
	 * so allocate exactly that many WORDs. The old *3 triple-counted and
	 * reserved 589,806 bytes per 98,301-index dynamic buffer instead of
	 * 196,602 (768 KiB recovered across the two). Write-bounds audit in
	 * docs/OPTIMIZATION-LOG.md: per-frame fills are MAXPOLYVERTS-capped
	 * (800 verts/call), text overlays write tens of quads, load-time
	 * loops write exactly triangleCount*3 — all far below requested. */
	renderObject->indexBuffer = linearAlloc(numIndices * sizeof(WORD));
	return renderObject->indexBuffer != nullptr;
}

bool FSCreateDynamicIndexBuffer(RENDEROBJECT *renderObject, int numIndices)
{
	return FSCreateIndexBuffer(renderObject, numIndices);
}

bool FSLockIndexBuffer(RENDEROBJECT *renderObject, WORD **indices)
{
	*indices = (WORD *)renderObject->indexBuffer;
	return true;
}

bool FSLockVertexBuffer(RENDEROBJECT *renderObject, LVERTEX **verts)
{
	*verts = (LVERTEX *)renderObject->vertexBuffer;
	return true;
}

bool FSUnlockIndexBuffer(RENDEROBJECT *renderObject)  { (void)renderObject; return true; }
bool FSUnlockVertexBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSLockNormalBuffer(RENDEROBJECT *renderObject, NORMAL **normals)
{
	*normals = (NORMAL *)renderObject->normalBuffer;
	return true;
}

bool FSUnlockNormalBuffer(RENDEROBJECT *renderObject) { (void)renderObject; return true; }

bool FSCreateDynamic2dVertexBuffer(RENDEROBJECT *renderObject, int numVertices)
{
	renderObject->vertexBuffer = linearAlloc(numVertices * sizeof(TLVERTEX));
	return renderObject->vertexBuffer != nullptr;
}

bool FSLockPretransformedVertexBuffer(RENDEROBJECT *renderObject, TLVERTEX **verts)
{
	*verts = (TLVERTEX*)renderObject->vertexBuffer;
	return true;
}

void FSReleaseRenderObject(RENDEROBJECT *renderObject)
{
	int i;
	if (renderObject->vertexBuffer)
	{
		linearFree(renderObject->vertexBuffer);
		renderObject->vertexBuffer = nullptr;
	}
	if (renderObject->normalBuffer)
	{
		linearFree(renderObject->normalBuffer);
		renderObject->normalBuffer = nullptr;
	}
	if (renderObject->indexBuffer)
	{
		linearFree(renderObject->indexBuffer);
		renderObject->indexBuffer = nullptr;
	}
	for (i = 0; i < renderObject->numTextureGroups; i++)
		renderObject->textureGroups[i].texture = nullptr;
	renderObject->numTextureGroups = 0;
}
