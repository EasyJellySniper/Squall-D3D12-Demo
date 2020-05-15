using System;
using System.Runtime.InteropServices;
using UnityEngine;
using UnityEngine.Rendering;

struct SubMesh
{
    /// <summary>
    /// index count
    /// </summary>
    public uint IndexCountPerInstance;

    /// <summary>
    /// start index
    /// </summary>
    public uint StartIndexLocation;

    /// <summary>
    /// start vertex
    /// </summary>
    public int BaseVertexLocation;
};

struct MeshData
{
    /// <summary>
    /// vertex buffer count
    /// </summary>
    public int vertexBufferCount;

    /// <summary>
    /// submesh count
    /// </summary>
    public int subMeshCount;

    /// <summary>
    /// submesh
    /// </summary>
    public SubMesh[] submesh;

    /// <summary>
    /// vertex buffer
    /// </summary>
    public IntPtr[] vertexBuffer;

    /// <summary>
    /// vertex size in byte
    /// </summary>
    public uint[] vertexSizeInBytes;

    /// <summary>
    /// vertex stride in byte
    /// </summary>
    public uint[] vertexStrideInBytes;

    /// <summary>
    /// index buffer
    /// </summary>
    public IntPtr indexBuffer;

    /// <summary>
    /// index size in byte
    /// </summary>
    public uint indexSizeInBytes;

    /// <summary>
    /// index format
    /// </summary>
    public int indexFormat;
};

/// <summary>
/// sq mesh filter
/// </summary>
[RequireComponent(typeof(MeshFilter))]
public class SqMeshFilter : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern bool AddMesh(int _instanceID, MeshData _meshData);

    MeshData meshData;
    Mesh mesh;

    /// <summary>
    /// main mesh
    /// </summary>
    public Mesh MainMesh { get { return mesh; } }

	void Start ()
    {
        mesh = GetComponent<MeshFilter>().sharedMesh;

        // setup basic info
        meshData.vertexBufferCount = mesh.vertexBufferCount;
        meshData.subMeshCount = mesh.subMeshCount;

        uint indexCount = 0;
        meshData.submesh = new SubMesh[mesh.subMeshCount];
        for (int i = 0; i < meshData.subMeshCount; i++)
        {
            meshData.submesh[i].BaseVertexLocation = (int)mesh.GetBaseVertex(i);
            meshData.submesh[i].IndexCountPerInstance = mesh.GetIndexCount(i);
            meshData.submesh[i].StartIndexLocation = mesh.GetIndexStart(i);
            indexCount += mesh.GetIndexCount(i);
        }

        // setup vertex buffer
        meshData.vertexBuffer = new IntPtr[mesh.vertexBufferCount];
        meshData.vertexSizeInBytes = new uint[mesh.vertexBufferCount];
        meshData.vertexStrideInBytes = new uint[mesh.vertexBufferCount];
        for (int i = 0; i < mesh.vertexBufferCount; i++)
        {
            CalcVertexData(mesh, ref meshData.vertexSizeInBytes[i], ref meshData.vertexStrideInBytes[i]);
            meshData.vertexBuffer[i] = mesh.GetNativeVertexBufferPtr(i);
        }

        // setup index buffer
        meshData.indexBuffer = mesh.GetNativeIndexBufferPtr();
        meshData.indexFormat = (int)mesh.indexFormat;
        if (mesh.indexFormat == IndexFormat.UInt16)
        {
            meshData.indexSizeInBytes = indexCount * 2;
        }
        else
        {
            meshData.indexSizeInBytes = indexCount * 4;
        }

        // add mesh to native plugin
        if (!AddMesh(mesh.GetInstanceID(), meshData))
        {
            Debug.LogError("[Error] SqMeshFilter: AddMesh() Failed.");
            enabled = false;
        }
    }

    void CalcVertexData(Mesh _mesh, ref uint _size, ref uint _stride)
    {
        // vertex layout:
        // float3 position (12 bytes) 
        // float3 normal (12 bytes) 
        // float4 tangent (16 bytes)
        // float2 uv(8 bytes)
        // float2 uv2(8 bytes)
        // it's good to share this layout for rendering
        // so that we can reduce the changing of pipeline state object

        // calculate possible size/stride for vertex
        _size = 0;
        _stride = 0;

        if (_mesh.vertexCount == 0)
        {
            _mesh.vertices = new Vector3[4];
        }

        if (_mesh.normals.Length == 0)
        {
            _mesh.normals = new Vector3[_mesh.vertexCount];
        }

        if (_mesh.colors32.Length > 0)
        {
            _mesh.colors32 = null;
        }

        if (_mesh.colors.Length > 0)
        {
            _mesh.colors = null;
        }

        if (_mesh.uv.Length == 0)
        {
            _mesh.uv = new Vector2[_mesh.vertexCount];
        }

        if (_mesh.uv2.Length == 0)
        {
            _mesh.uv2 = new Vector2[_mesh.vertexCount];
        }

        if (_mesh.uv3.Length > 0)
        {
            _mesh.uv3 = null;
        }

        if (_mesh.uv4.Length > 0)
        {
            _mesh.uv4 = null;
        }

        if (_mesh.tangents.Length == 0)
        {
            _mesh.tangents = new Vector4[_mesh.vertexCount];
        }

        // force to alignment to 56 bytes
        _stride = 56;
        _size = (uint)(56 * _mesh.vertexCount);
    }
}
