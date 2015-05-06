<?php

/**
 * @file
 * Additions to the Chado Stock node type to support germplasm better.
 */

/**
 * Load germplasm specific information to a node.
 */
function tripal_germplasm_node_load($nodes, $types) {

  if (in_array('chado_stock', $types)) {
    foreach($nodes as $nid => $node) {
      $node->germplasm = new stdClass();

      // Add the maternal & paternal parent for display in the overview.
      $maternal = tripal_get_germplasm_parent('maternal',$node->stock);
      if (is_object($maternal)) {
        $node->germplasm->maternal_parent = $maternal;
      }
      $paternal = tripal_get_germplasm_parent('paternal',$node->stock);
      if (is_object($paternal)) {
        $node->germplasm->paternal_parent = $paternal;
      }

      // Add all the relationships
      $progeny = tripal_get_germplasm_progeny($node->stock, 'all');
      if (!empty($progeny)) {
        usort($progeny, "tripal_germplasm_cmp_progeny_name");
        $node->germplasm->progeny = $progeny;
      }

      $nodes[$nid] = $node;
    }
  }

  return $nodes;
}

/**
 * Custom Sort: Sort progeny results by progeny name
 */
function tripal_germplasm_cmp_progeny_name($a, $b) {
  return strcmp($a['progeny']['name'], $b['progeny']['name']);
}

/**
 * Implements hook_form_FORM_ID_alter().
 *
 * Alter the stock node form to add germplasm specific fields and make it
 * easier to enter germplasm.
 */
function tripal_germplasm_form_chado_stock_node_form_alter(&$form, &$form_state, $form_id) {
  $node = $form['#node'];

  ///////////////////////////////////////////////
  // DEFAULT VALUES

  // 2) in the $form_state['values'] array which occurs on a failed validation or
  //    ajax callbacks from non submit form elements
  // 3) in the $form_state['input'] array which occurs on ajax callbacks from submit
  //    form elements and the form is being rebuilt
  $type_id = NULL;
  $default_maternal = '';
  $current_maternal_desc = '';
  $default_paternal = '';
  $current_paternal_desc = '';
  $default_ril_cross_num = '';
  $default_date_registered = NULL;
  $default_market_class = '';
  $default_cb_year = '';
  $default_cb_season = 0;

  // 1) as elements of the $node object.  This occurs when editing an existing stock
  if (property_exists($node, 'stock')) {
    $sname = $node->stock->name;
    $stock_id = $node->stock->stock_id;
    $type_id = $node->stock->type_id->cvterm_id;
    $organism_id = $node->stock->organism_id->organism_id;

    $type = tripal_get_cvterm(array('name' => 'is_selection_of'));
    $selection_type_id = NULL;
    if (isset($type->cvterm_id)) {
      $selection_type_id = $type->cvterm_id;
    }
    $relationship_types = array(
      'object_id' => array(
        tripal_get_germplasm_parent_type_id('maternal') => 'is_maternal_parent_of',
        tripal_get_germplasm_parent_type_id('paternal') => 'is_paternal_parent_of',
      ),
      'subject_id' => array(
        $selection_type_id => 'is_selection_of'
      ),
    );


    $maternal_parent = tripal_get_germplasm_parent('maternal',$node->stock);
    if (is_object($maternal_parent)) {
      if (isset($maternal_parent->nid)) {
        $default_maternal = '('.$maternal_parent->stock->stock_id.') '.$maternal_parent->stock->name;
        $current_maternal_desc = ' The current maternal parent is <em>' . l($maternal_parent->stock->name, 'node/' . $maternal_parent->nid) . '</em>.';
      }
      else {
        $default_maternal = '('.$maternal_parent->stock_id.') '.$maternal_parent->name;
        $current_maternal_desc = ' The current maternal parent is <em>' . $maternal_parent->name . '</em>.';
      }
    }

    $paternal_parent = tripal_get_germplasm_parent('paternal',$node->stock);
    if (is_object($paternal_parent)) {
      if (isset($paternal_parent->nid)) {
        $default_paternal = '('.$paternal_parent->stock->stock_id.') '.$paternal_parent->stock->name;
        $current_paternal_desc = ' The current paternal parent is <em>' . l($paternal_parent->stock->name, 'node/' . $paternal_parent->nid) . '</em>.';
      }
      else {
        $default_paternal = '('.$paternal_parent->stock_id.') '.$paternal_parent->name;
        $current_paternal_desc = ' The current paternal parent is <em>' . $paternal_parent->name . '</em>.';
      }
    }

    $rel = chado_generate_var(
      'stock_relationship',
      array(
        'type_id' => array('name' => 'is_selection_of'),
        'subject_id' => $node->stock->stock_id
      ),
      array(
        'include_fk' => array('object_id' => TRUE)
      )
    );
    if (isset($rel->object_id)) {
      $default_ril_cross_num = '(' . $rel->object_id->stock_id . ') ' . $rel->object_id->name;
    }

    $properties = chado_generate_var('stockprop',array('stock_id' => $node->stock->stock_id), array('return_array' => TRUE));
    $properties = (is_array($properties)) ? $properties : array();
    $property_types = array();
    foreach ($properties as $property) {
      switch ($property->type_id->name) {
        case 'variety_year_released':
          $default_date_registered = $property->value;
          $property_types['variety'][$property->type_id->cvterm_id] = 'variety_year_released';
          break;
        case 'market_class':
          $default_market_class = $property->value;
          $property_types['variety'][$property->type_id->cvterm_id] = 'market_class';
          break;
        case 'crossingblock_year':
          $default_cb_year = $property->value;
          $property_types['cross'][$property->type_id->cvterm_id] = 'crossingblock_year';
          break;
        case 'crossingblock_season':
          $default_cb_season = $property->value;
          $property_types['cross'][$property->type_id->cvterm_id] = 'crossingblock_season';
          break;
      }
    }
  }

  // 2) if we are re constructing the form from a failed validation or ajax callback
  // then use the $form_state['values'] values
  if (array_key_exists('values', $form_state)) {

    $type_id = $form_state['values']['type_id'];

    $default_ril_cross_num = $form_state['values']['ril_cross_number'];
    $default_date_registered = $form_state['values']['date_registered'];
    $default_market_class = $form_state['values']['market_class'];
    $default_cb_year = $form_state['values']['cb_year'];
    $default_cb_season = $form_state['values']['cb_season'];

  }

  // 3) if we are re building the form from after submission (from ajax call) then
  // the values are in the $form_state['input'] array
  if (array_key_exists('input', $form_state) and !empty($form_state['input'])) {

    $type_id = $form_state['values']['type_id'];

    $default_ril_cross_num = $form_state['input']['ril_cross_number'];
    $default_date_registered = $form_state['input']['date_registered'];
    $default_market_class = $form_state['input']['market_class'];
    $default_cb_year = $form_state['input']['cb_year'];
    $default_cb_season = $form_state['input']['cb_season'];
  }

  ///////////////////////////////////////////////
  // Form Proper.

  //---------------------------------------------
  // Move the core elements into a fieldset.
  $form['core'] = array(
    '#type' => 'fieldset',
    '#weight' => -10
  );

  $core_elements = array('sname', 'type_id', 'organism_id');
  foreach ($core_elements as $element_name) {
    $form['core'][$element_name] = $form[$element_name];
    unset($form[$element_name]);
  }

  // Hide notes.
  $form['stock_description']['#type'] = 'hidden';

  // Hide uniquename & main database reference since these will be set for them.
  $form['uniquename']['#type'] = 'hidden';
  $form['uniquename']['#required'] = FALSE;
  $form['uniquename']['#value'] = (isset($node->stock)) ? $node->stock->uniquename : uniqid('GERM:');
  $form['database_reference']['#type'] = 'hidden';

  // Add ajax to the type.
  $form['core']['type_id']['#ajax'] = array(
    'callback' => 'ajax_tripal_germplasm_node_form_type_specific_options',
    'wrapper' => 'type-specific-options',
  );

  //---------------------------------------------
  // Change the dbxref, properties & relationships forms to be in vertical tabs.
  $form['chado_node_api'] = array(
    '#type' => 'vertical_tabs',
    '#weight' => 50
  );
  $node_api_elements = array('addtl_dbxrefs', 'properties', 'relationships');
  foreach ($node_api_elements as $element_name) {
    $form[$element_name]['#group'] = 'chado_node_api';
    $form[$element_name]['#collapsible'] = TRUE;
    $form[$element_name]['#collapsed'] = TRUE;
    unset($form[$element_name]['#prefix']);
    unset($form[$element_name]['#suffix']);
  }

  // change the name of the additional dbxrefs fieldset.
  $form['addtl_dbxrefs']['#title'] = 'External Accessions';

  // Add in some themeing
  //---------------------------------------------
  $form['#attached']['css'][] = drupal_get_path('module','tripal_germplasm') . '/theme/css/tripal_germplasm.node_form.css';
  $form['#attached']['js'][] = drupal_get_path('module','tripal_germplasm') . '/theme/js/stockNode_relationshipJump.js';
  $form['#attached']['js'][] = drupal_get_path('module','tripal_germplasm') . '/theme/js/stockNode_propertyJump.js';

  //---------------------------------------------
  // Type-specific Additions.
  $form['type_specific'] = array(
    '#type' => 'markup',
    '#prefix' => '<div id="type-specific-options">',
    '#suffix' => '</div>'
  );

  $form['type_specifc']['property_type_ids'] = array(
    '#type' => 'hidden',
    '#value' => serialize($property_types),
  );

  $form['type_specific']['relationship_type_ids'] = array(
    '#type' => 'hidden',
    '#value' => serialize($relationship_types),
  );

  // Determine the category.
  $category = 'cross';
  if (tripal_is_germplasm_variety(array('type_id' => $type_id))) {
    $category = 'variety';
  }
  elseif (tripal_is_germplasm_ril(array('type_id' => $type_id))) {
    $category = 'ril';
  }
  $form['type_specifc']['type_category'] = array(
    '#type' => 'hidden',
    '#value' => $category,
  );

  // Cross
  //'''''''''''''''
  $form['type_specific']['cross'] = array(
    '#type' => 'fieldset',
    '#title' => 'Type-specific: Cross',
    '#weight' => 2,
    '#prefix' => '<div id="type-specific-options">',
    '#suffix' => '</div>'
  );
  if ($category != 'cross') {
    $form['type_specific']['cross']['#prefix'] = '<span style="display:none">';
    $form['type_specific']['cross']['#suffix'] = '</span>';
  }

  // Crossing Block Year
  $form['type_specific']['cross']['cb_year'] = array(
    '#type' => 'textfield',
    '#title' => 'Crossing Block Year',
    '#description' => 'The year this cross was made in (ie: '.date("Y").').',
    '#default_value' => $default_cb_year
  );

  $form['type_specific']['cross']['cb_season'] = array(
    '#type' => 'select',
    '#title' => 'Crossing Block Season',
    '#description' => 'The season this cross was made in.',
    '#options' => array(
      0 => '',
      'Spring' => 'Spring',
      'Summer' => 'Summer',
      'Fall' => 'Fall',
      'Winter' => 'Winter'
    ),
    '#default_value' => $default_cb_season
  );

  // Variety
  //'''''''''''''''
  $form['type_specific']['variety'] = array(
    '#type' => 'fieldset',
    '#title' => 'Type-specific: Variety',
    '#weight' => 3,
  );
  if ($category != 'variety') {
    $form['type_specific']['variety']['#prefix'] = '<span style="display:none">';
    $form['type_specific']['variety']['#suffix'] = '</span>';
  }

  // Date Registered
  $form['type_specific']['variety']['date_registered'] = array(
    '#type' => 'textfield',
    '#title' => 'Date Registered',
    '#description' => 'The date this variety was registered.',
    '#default_value' => $default_date_registered
  );

  // Market Class
  $form['type_specific']['variety']['market_class'] = array(
    '#type' => 'textfield',
    '#title' => 'Market Class',
    '#description' => 'The market class of this variety.',
    '#default_value' => $default_market_class
  );

  // RIL
  //'''''''''''''''
  $form['type_specific']['ril'] = array(
    '#type' => 'fieldset',
    '#title' => 'Type-specific: RIL',
    '#weight' => 4,
    '#prefix' => '<div id="type-specific-options">',
    '#suffix' => '</div>'
  );
  if ($category != 'ril') {
    $form['type_specific']['ril']['#prefix'] = '<span style="display:none">';
    $form['type_specific']['ril']['#suffix'] = '</span>';
  }

  // Original Cross Number
  $form['type_specific']['ril']['ril_cross_number'] = array(
    '#type' => 'textfield',
    '#title' => 'Original Cross Number',
    '#description' => 'The cross number the RIL originated from',
    '#default_value' => $default_ril_cross_num,
    '#autocomplete_path' => 'tripal_ajax/tripal_germplasm/name_to_id/ALL'
  );
  if (isset($node->stock->organism_id->genus)) {
    $form['type_specific']['ril']['ril_cross_number']['#autocomplete_path'] = 'tripal_ajax/tripal_germplasm/name_to_id/' . $node->stock->organism_id->genus;
  }

  // Date Deposited with number of lines
  // @TODO: Add compound form element with two textfields to enter two properties concurrently

  // Accession
  //'''''''''''''''
  /**
  $form['accession'] = array(
    '#type' => 'fieldset',
    '#title' => 'Accession Details',
    '#weight' => 5,
    '#collapsible' => TRUE,
    '#collapsed' => !tripal_is_germplasm_accession(array('type_id' => $type_id)),
  );

  // Dbxrefs
  // @TODO: Create a compound form field to enter both the db.name and accession
*/

  // Disable the rows in the chado node api: properties
  // that are now controlled by custom fields.
  //---------------------------------------------
  if (isset($property_types[$category])) {
    foreach ($property_types[$category] as $type_id => $property_name) {
      foreach ($form['properties']['property_table'][$type_id] as $key => $form_row) {
        if (!preg_match('/^#/', $key)) {
          unset($form['properties']['property_table'][$type_id][$key]['property_action']['#ajax']);
          $form['properties']['property_table'][$type_id][$key]['type']['#prefix'] = '** ';
          $form['properties']['property_table'][$type_id][$key]['#attributes']['class'][] = 'custom-control';
        }
      }
    }
  }

  // And notify the user of what's going on.
  // Both in the type-specific fieldsets
  $form['type_specific']['cross']['removal-warning'] = array(
    '#type' => 'markup',
    '#markup' => 'Remove a type-specific property by deleting the text in the corresponding box below.',
    '#prefix' => '<div id="chado-node-api-property-parent-warning" class="messages property-type-specific fieldset-warning tripal-germplasm-warning" style="display:none; height: 18px">',
    '#suffix' => '</div>',
    '#weight' => -10
  );
  $form['type_specific']['variety']['removal-warning'] = $form['type_specific']['cross']['removal-warning'];
  $form['type_specific']['ril']['removal-warning'] = $form['type_specific']['cross']['removal-warning'];

  // And notifty them in the chado node api fieldset as well.
  $form['properties']['type_specific_msg'] = array(
    '#type' => 'item',
    '#markup' => '** To add or remove <em>Type-Specific Properties</em> of the current germplasm, please use the "Type-Specific" section above.',
    '#prefix' => '<div id="chado-node-api-relationship-parent-warning" class="messages property-type-specific tripal-germplasm-warning">',
    '#suffix' => '</div>',
    '#weight' => 4
  );

  // Add additional validate to Chado Node Api Property "Add" button
  $form['properties']['property_table']['new']['property_action']['#validate'][] = 'tripal_germplasm_node_form_properties_add_button_validate';


  //---------------------------------------------
  // Parents
  $form['parents'] = array(
    '#type' => 'fieldset',
    '#title' => 'Parents',
    '#description' => 'The parents of the current germplasm (if known).'
  );

  if (isset($maternal_parent->stock_id)) {
    $form['parents']['maternal_parent_id'] = array(
      '#type' => 'hidden',
      '#value' => $maternal_parent->stock_id
    );
  }
  $form['parents']['maternal_parent'] = array(
    '#type' => 'textfield',
    '#title' => 'Maternal Parent',
    '#description' => 'The name of the maternal parent of this germplasm.' . $current_maternal_desc,
    '#default_value' => $default_maternal,
    '#autocomplete_path' => 'tripal_ajax/tripal_germplasm/name_to_id/ALL',
    '#weight' => 9
  );
  if (isset($node->stock->organism_id->genus)) {
    $form['parents']['maternal_parent']['#autocomplete_path'] = 'tripal_ajax/tripal_germplasm/name_to_id/' . $node->stock->organism_id->genus;
  }

  if (isset($paternal_parent->stock_id)) {
    $form['parents']['paternal_parent_id'] = array(
      '#type' => 'hidden',
      '#value' => $paternal_parent->stock_id
    );
  }
  $form['parents']['paternal_parent'] = array(
    '#type' => 'textfield',
    '#title' => 'Paternal Parent',
    '#description' => 'The name of the paternal parent of this germplasm.' . $current_paternal_desc,
    '#default_value' => $default_paternal,
    '#autocomplete_path' => 'tripal_ajax/tripal_germplasm/name_to_id/ALL',
    '#weight' => 10
  );
  if (isset($node->stock->organism_id->genus)) {
    $form['parents']['paternal_parent']['#autocomplete_path'] = 'tripal_ajax/tripal_germplasm/name_to_id/' . $node->stock->organism_id->genus;
  }

  // Make Chado Node API relationship table use our autocomplete.
  $form['relationships']['relationship_table']['new']['object_name']['#autocomplete_path'] = $form['parents']['maternal_parent']['#autocomplete_path'];
  $form['relationships']['relationship_table']['new']['subject_name']['#autocomplete_path'] = $form['parents']['maternal_parent']['#autocomplete_path'];

  // Disable the rows in the chado node api: relationships
  // that are now controlled by custom fields.
  //---------------------------------------------
  // Ensure they don't remove relationships they shouldn't.
  foreach ($relationship_types as $side => $types) {
    foreach ($types as $type_id) {
      foreach ($form['relationships']['relationship_table'][$type_id] as $key => $form_row) {
        if (!preg_match('/^#/', $key)) {
          if ($form_row[$side]['#value'] == $form_state['node']->stock->stock_id) {
            unset($form['relationships']['relationship_table'][$type_id][$key]['rel_action']['#ajax']);
            $form['relationships']['relationship_table'][$type_id][$key]['subject_name']['#prefix'] = '** ';
            $form['relationships']['relationship_table'][$type_id][$key]['#attributes']['class'][] = 'custom-control';
          }
        }
      }
    }
  }

  // And notify the user of what's going on.
  // Both in the parents fieldset
  $form['parents']['removal-warning'] = array(
    '#type' => 'markup',
    '#markup' => 'Remove a parent by deleting the text in the corresponding box below.',
    '#prefix' => '<div id="edit-parent-fieldset-relationship-warning" class="messages relationship-parent tripal-germplasm-warning fieldset-warning" style="display:none; height: 18px">',
    '#suffix' => '</div>'
  );

  // And notifty them in the chado node api fieldset as well.
  $form['relationships']['parent_msg'] = array(
    '#type' => 'item',
    '#markup' => '** To add or remove Parents of the current germplasm, please use the "Parents" section above.',
    '#prefix' => '<div id="chado-node-api-relationship-parent-warning" class="messages relationship-parent tripal-germplasm-warning">',
    '#suffix' => '</div>',
    '#weight' => 4
  );

  // Add additional validate to Chado Node Api Relationship "Add" button
  $form['relationships']['relationship_table']['new']['rel_action']['#validate'][] = 'tripal_germplasm_node_form_relationships_add_button_validate';

}

/**
 * AJAX Callback: replace the Type-specific options when the type drop-down
 * on the node form is changed
 */
function ajax_tripal_germplasm_node_form_type_specific_options($form, $form_state) {
  return $form['type_specific'];
}

/**
 * Validate for the Chado Node API: Properties Add Button.
 *
 * Purpose: To ensure that users don't add type-specific properties
 * and instead use the appropriate section.
 */
function tripal_germplasm_node_form_properties_add_button_validate($form, &$form_state) {

  $property_types = unserialize($form_state['values']['property_type_ids']);
  $new_type_id = $form_state['values']['property_table']['new']['type'];

  // Ensure that if a type has been selected that the user is not trying
  // to add a type-specific property.
  if ($new_type_id != 0) {
    if (isset($property_types[$form_state['values']['type_category']][$new_type_id])) {
      form_set_error(
        'property_table][new][type',
        t('Cannot add %type_name property because it is type-specific. Please add type-specific properties in the "type-specific" section above.',
          array('%type_name' => $property_types[$form_state['values']['type_category']][$new_type_id]))
      );
    }
  }
}

/**
 * Validate for the Chado Node API: Relationship Add Button.
 *
 * Purpose: To ensure that users don't add parental relationship
 * and instead use the parents section.
 */
function tripal_germplasm_node_form_relationships_add_button_validate($form, &$form_state) {

  $relationship_types = unserialize($form_state['values']['relationship_type_ids']);

  // Handle the original cross number.
  if ($form_state['values']['relationship_table']['new']['subject_is_current'] == TRUE) {
    if (in_array($form_state['values']['relationship_table']['new']['type_name'], $relationship_types['subject_id'])) {
      form_set_error('relationship_table][new][type_name',
        t('Cannot add %type_name relationship because it is type-specific. Please add this relationship in the "Type-Specific" section above, specifically it is the original cross number.',
          array('%type_name' => $form_state['values']['relationship_table']['new']['type_name']))
      );
    }
  }

  // Now handle parents.
  if ($form_state['values']['relationship_table']['new']['object_is_current'] == TRUE) {
    if (in_array($form_state['values']['relationship_table']['new']['type_name'], $relationship_types['object_id'])) {
      form_set_error('relationship_table][new][type_name',
        t('Cannot add %type_name relationship because it is a parental relationship. Please add this relationship in the "Parents" section above.',
          array('%type_name' => $form_state['values']['relationship_table']['new']['type_name']))
      );
    }
  }

}

/**
 * Chado Stock Node Form Validate
 *
 * Implements hook_node_validate().
 *
 * NOTE: In order to take advantage of the Chado Node API many of the form
 * elements added in tripal_germplasm_form_chado_stock_node_form_alter()
 * simply add/update their value in the property/dbxref/relationship_table
 * arrays added by the chado_stock_form().
 */
function tripal_germplasm_node_validate($node, $form, &$form_state) {
  if ($node->type == 'chado_stock') {

    // Handle Parents.
    //-------------------------------------------------------------------------
    // Ensure that the relationship fields are set based on the autocomplete
    // and if they are, add them to the relationship_table array.
    $type = tripal_get_cvterm(array('name' => 'is_selection_of'));
    $selection_type_id = NULL;
    if (isset($type->cvterm_id)) {
      $selection_type_id = $type->cvterm_id;
    }

    $relationship_fields = array(
      'maternal_parent' => array(
        'title' => 'Maternal Parent',
        'relationship' => array(
          'subject_id' => NULL,
          'type_id' => variable_get('germplasm_mparent_rel_type_id', NULL),
          'object_id' => NULL, //NOTE: Don't just use node->stock_id since this is not avail. on create
        ),
        'static_side' => 'object_id',
        'delete_if_empty' => TRUE
      ),
      'paternal_parent' => array(
        'title' => 'Paternal Parent',
        'relationship' => array(
          'subject_id' => NULL,
          'type_id' => variable_get('germplasm_pparent_rel_type_id', NULL),
          'object_id' => NULL, //NOTE: Don't just use node->stock_id since this is not avail. on create
        ),
        'static_side' => 'object_id',
        'delete_if_empty' => TRUE
      ),
      'ril_cross_number' => array(
        'title' => 'Original Cross Number',
        'relationship' => array(
          'subject_id' => NULL, //NOTE: Don't just use node->stock_id since this is not avail. on create
          'type_id' => $selection_type_id,
          'object_id' => NULL
        ),
        'static_side' => 'subject_id',
        'delete_if_empty' => FALSE
      ),
    );
    foreach ($relationship_fields as $field_name => $fields) {
      if (!empty($form_state['values'][$field_name]) AND !preg_match('/\((\d+)\).*/', $form_state['values'][$field_name], $matches)) {
        form_set_error($field_name, t('You need to choose the %title from the autocomplete rather than just typing in the full name manually.', array('%title' => $fields['title'])));
      }
      elseif (!empty($form_state['values'][$field_name])) {

        // Set the parent based on the regex match (stock_id)
        $fields['changing_side'] = ($fields['static_side'] == 'subject_id') ? 'object_id' : 'subject_id';
        $fields['relationship'][ $fields['changing_side'] ] = $matches[1];

        // Now add/update the parent in the form_state.
        tripal_germplasm_add_relationship_chado_node_form_reltable(
          $form_state,
          $fields['relationship'],
          $fields['changing_side']
        );
      }
      // If the maternal parent form element is empty, we might need to remove
      // a pre-existing parent.
      else {
        if ($fields['delete_if_empty'] AND isset($node->stock_id)) {

          // First we need to set the static side
          $fields['relationship'][$fields['static_side']] = $node->stock_id;

          // Only then can we delete.
          tripal_germplasm_remove_relationship_chado_node_form_reltable(
            $form_state,
            $fields['relationship'],
            $fields['static_side']
          );
        }
      }
    }

    // Now handle properties.
    //-------------------------------------------------------------------------
    // NOTE: the following code assumes that there will only ever be one stockprop
    // value for each type (ie: the rank field will not be used). As such,
    // if you have two occurances of a property added through another method
    // and someone empties that field then both properties of that type will be removed.

    // Retrieve Type_ids for the various properties.
    $variety_year_released_id = tripal_get_cvterm(array('name' => 'variety_year_released'));
    if (isset($variety_year_released_id->cvterm_id)) {
      $variety_year_released_id = $variety_year_released_id->cvterm_id;
    }
    $market_class_id = tripal_get_cvterm(array('name' => 'market_class'));
    if (isset($market_class_id->cvterm_id)) {
      $market_class_id = $market_class_id->cvterm_id;
    }

    // List the properties.
    $properties = array(
      'cb_year' => array(
        'title' => 'Crossing Block Year',
        'prop_type_id' => tripal_get_germplasm_crossingblock_type_id('year'),
        'prop_value' => $form_state['values']['cb_year'],
        'prop_rank' => 'TEMP' . uniqid()
      ),
      'cb_season' => array(
        'title' => 'Crossing Block Season',
        'prop_type_id' => tripal_get_germplasm_crossingblock_type_id('season'),
        'prop_value' => $form_state['values']['cb_season'],
        'prop_rank' => 'TEMP' . uniqid()
      ),
      'date_registered' => array(
        'title' => 'Date Variety Registered',
        'prop_type_id' => $variety_year_released_id,
        'prop_value' => $form_state['values']['date_registered'],
        'prop_rank' => 'TEMP' . uniqid()
      ),
      'market_class_id' => array(
        'title' => 'Market Class',
        'prop_type_id' => $market_class_id,
        'prop_value' => $form_state['values']['market_class'],
        'prop_rank' => 'TEMP' . uniqid()
      ),
    );
    // For each property needing to be updated/checked...
    foreach ($properties as $prop_fields) {
      if (!empty($prop_fields['prop_type_id'])) {

        // If there is a value then add it to the Chado Node API property table.
        if (!empty($prop_fields['prop_value'])) {
          $form_state['values']['property_table'][ $prop_fields['prop_type_id'] ] = array(array(
            'prop_type_id' => $prop_fields['prop_type_id'],
            'prop_value' => $prop_fields['prop_value'],
            'prop_rank' => $prop_fields['prop_rank']
          ));
        }
        // Otherwise remove it.
        else {
          unset($form_state['values']['property_table'][ $prop_fields['prop_type_id'] ]);
        }
      }
      // If there is no type_id set then we can't add/remove the properties
      // so warn the user/administrator.
      else {
        drupal_set_message(t('Unable to save %title', array('%title' => $prop_fields['title'])),'error');
        tripal_report_error(
          'tripal_germplasm',
          TRIPAL_ERROR,
          t('Unable to save %title when updating germplasm record. Specifically unable to find the stock property type_id',
            array('%title' => $prop_fields['title']))
        );
      }
    }

  } //End of if stock
}

/**
 * Implements hook_node_update().
 */
function tripal_germplasm_node_insert($node) {
  if ($node->type == 'chado_stock') {

    $stock_id = db_query("SELECT stock_id from chado_stock WHERE nid=:nid", array(':nid' => $node->nid))->fetchField();

    // Update the stock with the new uniquename
    // We can't set this initially since it depends on the stock_id
    // which is only available after the stock is created
    $stock = array();
    $prefix = variable_get('germplasm_prefix', 'GERM:');
    $stock['uniquename'] = $prefix . $stock_id;
    chado_update_record('stock', array('stock_id' => $stock_id), $stock);

    // Generate the primary dbxref
    $db_id = variable_get('tripal_stock_main_dbxref_db', 0);
    if ($db_id > 0) {
      $dbxref = tripal_insert_dbxref(array(
        'db_id' => $db_id,
        'accession' => $stock['uniquename'],
      ));

      // Update the stock to point to the new dbxref
      if (is_object($dbxref)) {
        $stock['dbxref_id'] = $dbxref->dbxref_id;
        chado_update_record('stock', array('stock_id' => $stock_id), $stock);
      }
    }
  }
}

/**
 * Implements hook_node_view().
 */
function tripal_germplasm_node_view($node, $view_mode, $langcode) {

  // Remove the tabs for properties, sysnonyms and references
  // since they are included in the overview.
  unset(
    $node->content['tripal_stock_properties'],
    $node->content['tripal_stock_synonyms'],
    $node->content['tripal_stock_references']
  );
}