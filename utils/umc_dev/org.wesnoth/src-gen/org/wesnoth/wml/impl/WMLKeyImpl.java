/**
 * <copyright>
 * </copyright>
 *

 */
package org.wesnoth.wml.impl;

import java.util.Collection;

import org.eclipse.emf.common.notify.Notification;
import org.eclipse.emf.common.notify.NotificationChain;

import org.eclipse.emf.common.util.EList;

import org.eclipse.emf.ecore.EClass;
import org.eclipse.emf.ecore.InternalEObject;

import org.eclipse.emf.ecore.impl.ENotificationImpl;

import org.eclipse.emf.ecore.util.EObjectContainmentEList;
import org.eclipse.emf.ecore.util.InternalEList;

import org.wesnoth.wml.WMLKey;
import org.wesnoth.wml.WMLKeyValue;
import org.wesnoth.wml.WmlPackage;

/**
 * <!-- begin-user-doc -->
 * An implementation of the model object '<em><b>WML Key</b></em>'.
 * <!-- end-user-doc -->
 * <p>
 * The following features are implemented:
 * <ul>
 *   <li>{@link org.wesnoth.wml.impl.WMLKeyImpl#getValue <em>Value</em>}</li>
 *   <li>{@link org.wesnoth.wml.impl.WMLKeyImpl#getEol <em>Eol</em>}</li>
 *   <li>{@link org.wesnoth.wml.impl.WMLKeyImpl#get_cardinality <em>cardinality</em>}</li>
 *   <li>{@link org.wesnoth.wml.impl.WMLKeyImpl#is_Enum <em>Enum</em>}</li>
 *   <li>{@link org.wesnoth.wml.impl.WMLKeyImpl#is_Translatable <em>Translatable</em>}</li>
 * </ul>
 * </p>
 *
 * @generated
 */
public class WMLKeyImpl extends WMLExpressionImpl implements WMLKey
{
  /**
   * The cached value of the '{@link #getValue() <em>Value</em>}' containment reference list.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #getValue()
   * @generated
   * @ordered
   */
  protected EList<WMLKeyValue> value;

  /**
   * The default value of the '{@link #getEol() <em>Eol</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #getEol()
   * @generated
   * @ordered
   */
  protected static final String EOL_EDEFAULT = "";

  /**
   * The cached value of the '{@link #getEol() <em>Eol</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #getEol()
   * @generated
   * @ordered
   */
  protected String eol = EOL_EDEFAULT;

  /**
   * The default value of the '{@link #get_cardinality() <em>cardinality</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #get_cardinality()
   * @generated
   * @ordered
   */
  protected static final char _CARDINALITY_EDEFAULT = ' ';

  /**
   * The cached value of the '{@link #get_cardinality() <em>cardinality</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #get_cardinality()
   * @generated
   * @ordered
   */
  protected char _cardinality = _CARDINALITY_EDEFAULT;

  /**
   * The default value of the '{@link #is_Enum() <em>Enum</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #is_Enum()
   * @generated
   * @ordered
   */
  protected static final boolean _ENUM_EDEFAULT = false;

  /**
   * The cached value of the '{@link #is_Enum() <em>Enum</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #is_Enum()
   * @generated
   * @ordered
   */
  protected boolean _Enum = _ENUM_EDEFAULT;

  /**
   * The default value of the '{@link #is_Translatable() <em>Translatable</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #is_Translatable()
   * @generated
   * @ordered
   */
  protected static final boolean _TRANSLATABLE_EDEFAULT = false;

  /**
   * The cached value of the '{@link #is_Translatable() <em>Translatable</em>}' attribute.
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @see #is_Translatable()
   * @generated
   * @ordered
   */
  protected boolean _Translatable = _TRANSLATABLE_EDEFAULT;

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  protected WMLKeyImpl()
  {
    super();
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  protected EClass eStaticClass()
  {
    return WmlPackage.Literals.WML_KEY;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public EList<WMLKeyValue> getValue()
  {
    if (value == null)
    {
      value = new EObjectContainmentEList<WMLKeyValue>(WMLKeyValue.class, this, WmlPackage.WML_KEY__VALUE);
    }
    return value;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public String getEol()
  {
    return eol;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public void setEol(String newEol)
  {
    String oldEol = eol;
    eol = newEol;
    if (eNotificationRequired())
      eNotify(new ENotificationImpl(this, Notification.SET, WmlPackage.WML_KEY__EOL, oldEol, eol));
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public char get_cardinality()
  {
    return _cardinality;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public void set_cardinality(char new_cardinality)
  {
    char old_cardinality = _cardinality;
    _cardinality = new_cardinality;
    if (eNotificationRequired())
      eNotify(new ENotificationImpl(this, Notification.SET, WmlPackage.WML_KEY__CARDINALITY, old_cardinality, _cardinality));
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Enum()
  {
    return _Enum;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public void set_Enum(boolean new_Enum)
  {
    boolean old_Enum = _Enum;
    _Enum = new_Enum;
    if (eNotificationRequired())
      eNotify(new ENotificationImpl(this, Notification.SET, WmlPackage.WML_KEY__ENUM, old_Enum, _Enum));
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Translatable()
  {
    return _Translatable;
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public void set_Translatable(boolean new_Translatable)
  {
    boolean old_Translatable = _Translatable;
    _Translatable = new_Translatable;
    if (eNotificationRequired())
      eNotify(new ENotificationImpl(this, Notification.SET, WmlPackage.WML_KEY__TRANSLATABLE, old_Translatable, _Translatable));
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Required()
  {
    return _cardinality == '1';
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Forbidden()
  {
    return _cardinality == '-';
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Optional()
  {
    return _cardinality == '?';
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  public boolean is_Repeatable()
  {
    return _cardinality == '*';
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  public NotificationChain eInverseRemove(InternalEObject otherEnd, int featureID, NotificationChain msgs)
  {
    switch (featureID)
    {
      case WmlPackage.WML_KEY__VALUE:
        return ((InternalEList<?>)getValue()).basicRemove(otherEnd, msgs);
    }
    return super.eInverseRemove(otherEnd, featureID, msgs);
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  public Object eGet(int featureID, boolean resolve, boolean coreType)
  {
    switch (featureID)
    {
      case WmlPackage.WML_KEY__VALUE:
        return getValue();
      case WmlPackage.WML_KEY__EOL:
        return getEol();
      case WmlPackage.WML_KEY__CARDINALITY:
        return get_cardinality();
      case WmlPackage.WML_KEY__ENUM:
        return is_Enum();
      case WmlPackage.WML_KEY__TRANSLATABLE:
        return is_Translatable();
    }
    return super.eGet(featureID, resolve, coreType);
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @SuppressWarnings("unchecked")
  @Override
  public void eSet(int featureID, Object newValue)
  {
    switch (featureID)
    {
      case WmlPackage.WML_KEY__VALUE:
        getValue().clear();
        getValue().addAll((Collection<? extends WMLKeyValue>)newValue);
        return;
      case WmlPackage.WML_KEY__EOL:
        setEol((String)newValue);
        return;
      case WmlPackage.WML_KEY__CARDINALITY:
        set_cardinality((Character)newValue);
        return;
      case WmlPackage.WML_KEY__ENUM:
        set_Enum((Boolean)newValue);
        return;
      case WmlPackage.WML_KEY__TRANSLATABLE:
        set_Translatable((Boolean)newValue);
        return;
    }
    super.eSet(featureID, newValue);
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  public void eUnset(int featureID)
  {
    switch (featureID)
    {
      case WmlPackage.WML_KEY__VALUE:
        getValue().clear();
        return;
      case WmlPackage.WML_KEY__EOL:
        setEol(EOL_EDEFAULT);
        return;
      case WmlPackage.WML_KEY__CARDINALITY:
        set_cardinality(_CARDINALITY_EDEFAULT);
        return;
      case WmlPackage.WML_KEY__ENUM:
        set_Enum(_ENUM_EDEFAULT);
        return;
      case WmlPackage.WML_KEY__TRANSLATABLE:
        set_Translatable(_TRANSLATABLE_EDEFAULT);
        return;
    }
    super.eUnset(featureID);
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  public boolean eIsSet(int featureID)
  {
    switch (featureID)
    {
      case WmlPackage.WML_KEY__VALUE:
        return value != null && !value.isEmpty();
      case WmlPackage.WML_KEY__EOL:
        return EOL_EDEFAULT == null ? eol != null : !EOL_EDEFAULT.equals(eol);
      case WmlPackage.WML_KEY__CARDINALITY:
        return _cardinality != _CARDINALITY_EDEFAULT;
      case WmlPackage.WML_KEY__ENUM:
        return _Enum != _ENUM_EDEFAULT;
      case WmlPackage.WML_KEY__TRANSLATABLE:
        return _Translatable != _TRANSLATABLE_EDEFAULT;
    }
    return super.eIsSet(featureID);
  }

  /**
   * <!-- begin-user-doc -->
   * <!-- end-user-doc -->
   * @generated
   */
  @Override
  public String toString()
  {
    if (eIsProxy()) return super.toString();

    StringBuffer result = new StringBuffer(super.toString());
    result.append(" (eol: ");
    result.append(eol);
    result.append(", _cardinality: ");
    result.append(_cardinality);
    result.append(", _Enum: ");
    result.append(_Enum);
    result.append(", _Translatable: ");
    result.append(_Translatable);
    result.append(')');
    return result.toString();
  }

} //WMLKeyImpl
