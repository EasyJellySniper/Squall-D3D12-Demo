using System;
using System.Runtime.InteropServices;
using UnityEngine;

/// <summary>
/// material constant
/// </summary>
[StructLayout(LayoutKind.Sequential)]
public struct MaterialConstant
{
    public Vector4 _MainTex_ST;
    public float _CutOff;
    public int _DiffuseIndex;
    public int _DiffuseSampler;
    public float _Padding;
};

public class SqMaterial
{
    [DllImport("SquallGraphics")]
    static extern int AddNativeTexture(int _texID, IntPtr _texture);

    [DllImport("SquallGraphics")]
    static extern int AddNativeSampler(TextureWrapMode _wrapModeU, TextureWrapMode _wrapModeV, TextureWrapMode _wrapModeW, int _anisoLevel);

    /// <summary>
    /// instance
    /// </summary>
    private static SqMaterial instance = null;

    /// <summary>
    /// singleton instance
    /// </summary>
    public static SqMaterial Instance
    {
        get
        {
            if (instance == null)
            {
                instance = new SqMaterial();
                instance.Init();
            }
            return instance;
        }
    }

    private SqMaterial()
    {
        // prevent constructor
    }

    ~SqMaterial()
    {
        Release();
    }

    /// <summary>
    /// constant size
    /// </summary>
    public int matConstantSize;

    /// <summary>
    /// white tex
    /// </summary>
    Texture2D whiteTex;

    /// <summary>
    /// get material constant
    /// </summary>
    /// <param name="_mat"> mat </param>
    /// <returns> material constant </returns>
    public MaterialConstant GetMaterialConstant(Material _mat)
    {
        MaterialConstant mc = new MaterialConstant();

        Texture2D albedo = _mat.mainTexture as Texture2D;
        if (albedo)
        {
            mc._DiffuseIndex = AddNativeTexture(albedo.GetInstanceID(), albedo.GetNativeTexturePtr());
            mc._DiffuseSampler = AddNativeSampler(albedo.wrapModeU, albedo.wrapModeV, albedo.wrapModeW, SqGraphicManager.instance.globalAnisoLevel);
        }
        else
        {
            mc._DiffuseIndex = AddNativeTexture(whiteTex.GetInstanceID(), whiteTex.GetNativeTexturePtr());
            mc._DiffuseSampler = AddNativeSampler(whiteTex.wrapModeU, whiteTex.wrapModeV, whiteTex.wrapModeW, SqGraphicManager.instance.globalAnisoLevel);
        }

        mc._MainTex_ST = new Vector4(_mat.mainTextureScale.x, _mat.mainTextureScale.y, _mat.mainTextureOffset.x, _mat.mainTextureOffset.y);
        if (_mat.HasProperty("_Cutoff"))
        {
            mc._CutOff = _mat.GetFloat("_Cutoff");
        }

        return mc;
    }

    void Init()
    {
        matConstantSize = Marshal.SizeOf(typeof(MaterialConstant));
        whiteTex = new Texture2D(1, 1);
        whiteTex.name = "Sq Default White";
        whiteTex.SetPixel(0, 0, Color.white);
        whiteTex.Apply();
    }

    void Release()
    {
        if (whiteTex)
        {
            GameObject.DestroyImmediate(whiteTex);
        }
    }
}
