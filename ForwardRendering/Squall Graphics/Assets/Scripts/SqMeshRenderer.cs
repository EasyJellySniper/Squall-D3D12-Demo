using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// sq mesh renderer
/// </summary>
[RequireComponent(typeof(SqMeshFilter))]
[RequireComponent(typeof(MeshRenderer))]
public class SqMeshRenderer : MonoBehaviour
{
    [DllImport("SquallGraphics")]
    static extern bool AddRenderer(int _instanceID, int _meshInstanceID);

    void Start ()
    {
        AddRenderer(GetInstanceID(), GetComponent<SqMeshFilter>().MainMesh.GetInstanceID());
	}
}
