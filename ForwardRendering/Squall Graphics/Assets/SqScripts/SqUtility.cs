using UnityEngine;

/// <summary>
/// utility class
/// </summary>
public class SqUtility 
{
    /// <summary>
    /// destroy RT safely
    /// </summary>
    /// <param name="_rt">rt</param>
    public static void SafeDestroyRT(ref RenderTexture _rt)
    {
        if (_rt)
        {
            _rt.Release();
            Object.DestroyImmediate(_rt);
        }
    }

    /// <summary>
    /// create RT
    /// </summary>
    /// <param name="_w">width</param>
    /// <param name="_h">height</param>
    /// <param name="_d">depth</param>
    /// <param name="_format">format</param>
    public static RenderTexture CreateRT(int _w, int _h, int _d, RenderTextureFormat _format, string _name, bool _uav = false)
    {
        RenderTexture rt = new RenderTexture(_w, _h, _d, _format, RenderTextureReadWrite.Linear);
        rt.name = _name;
        rt.enableRandomWrite = _uav;
        rt.Create();

        return rt;
    }
}
