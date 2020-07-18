using System;
using System.Runtime.InteropServices;
using UnityEngine;

// sq sky box
[RequireComponent(typeof(SqMeshFilter))]
public class SqSkybox : MonoBehaviour
{
    /// <summary>
    /// ambient ground
    /// </summary>
    [Header("Hemisphere Ambient Settings")]
    [ColorUsage(true, true)]
    public Color ambientGround = Color.gray / 5;

    /// <summary>
    /// ambient sky
    /// </summary>
    [ColorUsage(true, true)]
    public Color ambientSky = new Color(135 / 255f, 206 / 255f, 235 / 255f);

    /// <summary>
    /// skybox
    /// </summary>
    public Cubemap skybox;

    [DllImport("SquallGraphics")]
    static extern void SetAmbientLight(Vector4 _ag, Vector4 _as);

    [DllImport("SquallGraphics")]
    static extern void SetSkybox(IntPtr _skybox, TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel, int _meshId);

    void Start()
    {
        SetAmbientLight(ambientGround, ambientSky);
        SetSkybox(skybox.GetNativeTexturePtr(), skybox.wrapModeU, skybox.wrapModeV, skybox.wrapModeW, SqGraphicManager.instance.globalAnisoLevel, GetComponent<SqMeshFilter>().MainMesh.GetInstanceID());
    }

#if UNITY_EDITOR
    void OnValidate()
    {
        SetAmbientLight(ambientGround, ambientSky);
    }
#endif
}
