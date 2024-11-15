<?php

/**
 * @file
 * Admin callbacks for islandora_pathauto.
 */

/**
 * Admin form.
 */
function islandora_pathauto_admin_settings(array $form, array &$form_state) {
  module_load_include('inc', 'islandora', 'includes/utilities');
  $all_cmodels = islandora_get_content_models();
  $already_chosen = variable_get('islandora_pathauto_selected_cmodels', array());

  foreach ($already_chosen as $chosen) {
    $value = isset($all_cmodels[$chosen]) ? $all_cmodels[$chosen] : array('pid' => '', 'label' => '');
    unset($all_cmodels[$chosen]);
    $all_cmodels = array($chosen => $value) + $all_cmodels;
  }

  $form['content_model_wrapper'] = array(
    '#type' => 'fieldset',
    '#title' => t('Configure which content models are exposed to custom aliases'),
    '#collapsible' => FALSE,
    '#collapsed' => FALSE,
  );
  $form['content_model_wrapper']['pathauto_table'] = array(
    '#type' => 'item',
    '#title' => t('Enable custom pathauto settings for these content models'),
    '#description' => t('Configure aliases under Pathauto config: !link', array(
      '!link' => l(t('admin/config/search/path/patterns'), 'admin/config/search/path/patterns'),
    )),
    '#tree' => TRUE,
    '#theme' => 'islandora_pathauto_admin_table',
  );
  $alink_options = array(
    'attributes' => array('target' => '_blank'),
    'html' => TRUE,
  );
  $options = array();

  foreach ($all_cmodels as $cmodel) {
    $pid = $cmodel['pid'];
    $label = $cmodel['label'];
    $options[$pid] = '';
    $form['content_model_wrapper']['pathauto_table']['name'][$pid] = array(
      '#type' => 'item',
      '#markup' => l($label, "islandora/object/$pid", $alink_options),
    );
    $form['content_model_wrapper']['pathauto_table']['content_model'][$pid] = array(
      '#type' => 'item',
      '#markup' => $pid,
    );
  }
  $form['content_model_wrapper']['pathauto_table']['enabled'] = array(
    '#type' => 'checkboxes',
    '#options' => $options,
    '#default_value' => $already_chosen,
  );

  $options = array('sparql' => t('Sparql'));
  if (module_exists('islandora_solr')) {
    $options['solr'] = t('Solr');
  }
  $form['objects_query'] = array(
    '#type' => 'fieldset',
    '#title' => t('Objects Query Method'),
  );
  $form['objects_query']['objects_query_method'] = array(
    '#type' => 'radios',
    '#options' => $options,
    '#default_value' => variable_get('islandora_pathauto_objects_query_method', 'sparql'),
  );
  $form['objects_query']['objects_query_solr'] = array(
    '#type' => 'textfield',
    '#title' => t('Solr query'),
    '#description' => t('The Solr query to use to find all objects. In most cases *:* is good, but for multisites you may want to filter by ancestors_ms:"root_collection_id".'),
    '#states' => array(
      'visible' => array('input[name=objects_query_method]' => array('value' => 'solr')),
    ),
    '#default_value' => variable_get('islandora_pathauto_objects_query_solr', '*:*'),
  );

  $form['multi_server'] = array(
    '#type' => 'fieldset',
    '#title' => t('Multiple server configuration'),
    '#description' => t('When using multiple servers, one might want to trigger the alias generation on a different server when an object is ingested.'),
  );
  $triggertype = variable_get('islandora_pathauto_trigger_type', NULL);
  $triggerurl = variable_get('islandora_pathauto_trigger_url', NULL);
  $triggersecret = variable_get('islandora_pathauto_trigger_secret', NULL);
  $form['multi_server']['trigger_type'] = array(
    '#type' => 'select',
    '#title' => t('Trigger type'),
    '#options' => array(
       0 => t('Disabled'),
       'receiving' => t('Trigger receiving server'),
       'sending' => t('Trigger sending server'),
    ),
    '#default_value' => $triggertype,
    '#description' => t("Only enable for multi server configurations. For example: set the ingest server to 'trigger sending server' and the front-end server to 'trigger receiving server'.\nThe 'trigger sending server' will use the 'trigger URL' to send a trigger when an alias is created. The 'trigger receiving server' receives this trigger and creates an alias for the same object.\nDo not set both servers to 'trigger sending server' because this will create an endless loop."),
  );
  $form['multi_server']['trigger_url'] = array(
    '#type' => 'textfield',
    '#title' => t('Trigger URL'),
    '#description' => t('The base URL of the server to send a trigger to.'),
    '#states' => array(
      'visible' => array(':input[name=trigger_type]' => array('value' => 'sending')),
    ),
    '#default_value' => $triggerurl,
  );
  $form['multi_server']['trigger_secret'] = array(
    '#type' => 'textfield',
    '#title' => t('Trigger secret'),
    '#description' => t('A secret that is shared between the sending and receiving server. Use the same secret for both sending and receiving servers.'),
    '#states' => array(
      'visible' => array(':input[name=trigger_type]' => array(array('value' => 'sending'), array('value' => 'receiving'))),
    ),
    '#default_value' => $triggersecret,
  );

  $form['use_cron'] = array(
    '#type' => 'checkbox',
    '#title' => t('Use cron to automatically add / update aliases.'),
    '#default_value' => variable_get('islandora_pathauto_use_cron', FALSE),
  );

  $form['actions'] = array(
    '#type' => 'actions',
  );
  $form['actions']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save Configuration'),
    '#weight' => 0,
    '#submit' => array('islandora_pathauto_admin_settings_submit'),
  );
  return $form;
}

/**
 * Validate handler for admin form.
 */
function islandora_pathauto_admin_settings_validate($form, &$form_state) {
  $triggertype = $form_state['values']['trigger_type'];
  if ($triggertype === 'receiving') {
    if (empty($form_state['values']['trigger_secret'])) {
      form_set_error('trigger_secret', t("Fill out a secret shared with the sending server."));
    }
  }
  if ($triggertype === 'sending') {
    if (empty($form_state['values']['trigger_secret'])) {
      form_set_error('trigger_secret', t("Fill out a secret shared with the receiving server."));
    }
    if (empty($form_state['values']['trigger_url'])) {
      form_set_error('trigger_url', t("Fill out the URL of the receiving server."));
    }
    else {
      $response = islandora_pathauto_trigger_creation_on_server(NULL, 'insert', $form_state['values']['trigger_url'], $form_state['values']['trigger_secret']);
      if ($response === NULL) {
        form_set_error('trigger_url', t("The URL provided is not a valid receiving server. Make sure the URL contains only protocol and domain."));
      }
      elseif (isset($response['error'])) {
        form_set_error('trigger_url', t("The URL provided gave the following error: %error", array('%error' =>  $response['error'])));
      }
      elseif (isset($response['success']) && !$response['success']) {
        form_set_error('trigger_url', t("The URL provided is not valid and something unexpected happened"));
      }
    }
  }
}

/**
 * Submit handler for admin form.
 */
function islandora_pathauto_admin_settings_submit($form, &$form_state) {
  $enabled = array_filter($form_state['values']['pathauto_table']['enabled']);
  variable_set('islandora_pathauto_selected_cmodels', array_keys($enabled));
  variable_set('islandora_pathauto_use_cron', $form_state['values']['use_cron']);
  variable_set('islandora_pathauto_objects_query_method', $form_state['values']['objects_query_method']);
  variable_set('islandora_pathauto_objects_query_solr', $form_state['values']['objects_query_solr']);
  variable_set('islandora_pathauto_trigger_type', $form_state['values']['trigger_type']);
  variable_set('islandora_pathauto_trigger_url', $form_state['values']['trigger_url']);
  variable_set('islandora_pathauto_trigger_secret', $form_state['values']['trigger_secret']);
}

/**
 * Themes the form table.
 *
 * @param array $variables
 *   An array containing variables used in the theme.
 *
 * @return string
 *   The marked up output.
 */
function theme_islandora_pathauto_admin_table(array $variables) {
  // Set form.
  $form = $variables['form'];
  $rows = array();
  foreach ($form['name'] as $key => $element) {
    // Do not take form control structures.
    if (is_array($element) && element_child($key)) {
      $row = array();
      $row[] = array(
        'data' => drupal_render($form['enabled'][$key]),
        'class' => 'islandora-annotation-checkbox');
      $row[] = array(
        'data' => drupal_render($form['name'][$key]),
        'title' => $key);
      $row[] = array(
        'data' => drupal_render($form['content_model'][$key]));
      // Add to rows.
      $rows[] = array(
        'data' => $row,
        'class' => array('draggable'));
    }
  }
  // Individual table headers.
  $header = array();
  $header[] = array('data' => t('Enabled'));
  $header[] = array('data' => t('Content Model'));
  $header[] = array('data' => t('PID'));

  // Render form.
  $output = '';
  $output .= theme('table', array(
    'header' => $header,
    'rows' => $rows,
    'attributes' => array(
      'id' => 'annotation_admin_table',
    ),
  ));
  $output .= drupal_render_children($form);
  return $output;
}
