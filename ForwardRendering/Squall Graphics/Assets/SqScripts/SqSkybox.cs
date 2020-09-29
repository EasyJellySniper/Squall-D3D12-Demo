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

    /// <summary>
    /// sky intensity
    /// </summary>
    [Range(0f, 5f)]
    public float skyIntensity = 1;

    /// <summary>
    /// reflection distance
    /// </summary>
    public float reflectionDistance = 300;

    [DllImport("SquallGraphics")]
    static extern void SetAmbientLight(Vector4 _ag, Vector4 _as, float _skyIntensity, float _reflectionDistance);

    [DllImport("SquallGraphics")]
    static extern void SetSkybox(IntPtr _skybox, TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel, int _meshId);

    [DllImport("SquallGraphics")]
    static extern void SetSkyboxWorld(Matrix4x4 _world);

    void Start()
    {
        SetAmbientLight(ambientGround, ambientSky, skyIntensity, reflectionDistance);
        SetSkybox(skybox.GetNativeTexturePtr(), skybox.wrapModeU, skybox.wrapModeV, skybox.wrapModeW, SqGraphicManager.Instance.globalAnisoLevel, GetComponent<SqMeshFilter>().MainMesh.GetInstanceID());
        transform.hasChanged = true;
    }

#if UNITY_EDITOR
    void OnValidate()
    {
        SetAmbientLight(ambientGround, ambientSky, skyIntensity, reflectionDistance);
    }
#endif

    void Update()
    {
        if (transform.hasChanged)
        {
            SetSkyboxWorld(transform.localToWorldMatrix);
            transform.hasChanged = false;
        }
    }
}
